/*-
 * Copyright (c) 2019 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "cheric.h"
#include "net.h"
#include "ansi_escapes.h"
#include "unistd.h"

// Implements multiplayer snake over TCP

#define SNAKE_PORT 80

#define MAX_PLAYERS 0x10
#define BWIDTH  0x40
#define BHEIGHT 0x40

#ifdef HARDWARE_qemu
    #define GAME_SPEED MS_TO_CLOCK(100)
#else
// runs wierdly slow on FPGA. Probably sleep logic, CPU is not utilised...
    #define GAME_SPEED MS_TO_CLOCK(40)
#endif

#define PLAYER_NAME_LEN 16


#define BSIZE (BWIDTH * BHEIGHT)

#define INDX(X,Y) (((Y) * BWIDTH) + (X))

#define X_I(IND) (IND % BWIDTH)
#define Y_I(IND) (IND / BWIDTH)

#define EMPTY_SLOT 0xffff
#define HEAD_SLOT 0xfffe

typedef uint16_t board_t;

typedef struct {
    ssize_t dx;
    ssize_t dy;
} diff_t;

typedef enum state {
    getting_name = 0,
    need_spawn = 1,
    playing = 2,
    need_kill = 3,
} state_e;

struct player {
    ssize_t head;
    ssize_t tail;
    NET_SOCK ns;
    diff_t diff;
    state_e state;
    uint16_t mod;
    uint16_t name_index;
    char name[PLAYER_NAME_LEN];
} players[MAX_PLAYERS];
size_t free_hd = 0;

#define FOR_PLAYER(plyr) for(struct player* plyr = players; plyr != players+free_hd; plyr++)


#define SET_BACK(ns, clr) fprintf((FILE*)ns, ANSI_ESC_C clr);
void display_c(NET_SOCK ns, ssize_t indx, char c1, char c2) {
    unsigned x = (unsigned)((X_I(indx) * 2) + 2);
    unsigned y = (unsigned)(Y_I(indx) + 2);
    fprintf((FILE*)ns, ANSI_ESC_C "%d;%d" ANSI_SET_CURSOR "%c%c", y, x, c1, c2);
}

void display_board(NET_SOCK ns, board_t* board, ssize_t food) {
    SET_BACK(ns, ANSI_BACK_BLUE);
    fprintf((FILE*)ns, ANSI_ESC_C ANSI_CURSOR_HOME ANSI_ESC_C ANSI_ERASE);
    fputc('+', (FILE*)ns);
    for(size_t i = 0; i != 2*BWIDTH; i++) {
        fputc('-', (FILE*)ns);
    }
    fputc('+', (FILE*)ns);
    fputc('\n', (FILE*)ns);


    for(size_t i = 0; i != BHEIGHT; i++) {
        fputc('|', (FILE*)ns);
        SET_BACK(ns, ANSI_BACK_BLACK);
        int is_red = 0;
        for(size_t j = 0; j != BWIDTH; j++) {
            if(board[j] != EMPTY_SLOT) {
                if(!is_red) {
                    SET_BACK(ns, ANSI_BACK_RED);
                    is_red = 1;
                }
            } else {
                if(is_red) {
                    SET_BACK(ns, ANSI_BACK_BLACK);
                    is_red = 0;
                }

            }

            fputc(' ', (FILE*)ns);
            fputc(' ', (FILE*)ns);
        }
        SET_BACK(ns, ANSI_BACK_BLUE);
        fputc('|', (FILE*)ns);
        fputc('\n', (FILE*)ns);
    }

    fputc('+', (FILE*)ns);
    for(size_t i = 0; i != BWIDTH*2; i++) {
        fputc('-', (FILE*)ns);
    }
    fputc('+', (FILE*)ns);
    fputc('\n', (FILE*)ns);

    SET_BACK(ns, ANSI_BACK_BLACK)
    display_c(ns, food, '@', '@');
}

extern ssize_t TRUSTED_CROSS_DOMAIN(snake_ful) (capability arg, char* buf, uint64_t offset, uint64_t length);
__used ssize_t snake_ful(capability arg, char* buf, __unused uint64_t offset, uint64_t length) {
    diff_t* diff = &((struct player*)arg)->diff;
    char c = buf[length-1];
    if(c <= 'Z') c += 'z' - 'Z';
    switch(c) {
        case 'w':
            diff->dx = 0;
            diff->dy = -1;
            break;
        case 'a':
            diff->dx = -1;
            diff->dy = 0;
            break;
        case 's':
            diff->dx = 0;
            diff->dy = 1;
            break;
        case 'd':
            diff->dx = 1;
            diff->dy = 0;
            break;
        default:
            break;
    }
    return length;
}

extern ssize_t TRUSTED_CROSS_DOMAIN(snake_getname) (capability arg, char* buf, uint64_t offset, uint64_t length);
__used ssize_t snake_getname(capability arg, char* buf, __unused uint64_t offset, uint64_t length) {
    struct player* p = ((struct player*)arg);
    size_t consumed = 0;

    if(p->state != getting_name) return 0;

    do {
        char c = buf[consumed++];
        if(c != '\n' && p->mod != PLAYER_NAME_LEN) {
            p->name[p->mod++] = c;
        } else {
            if(p->mod == 0) {
                memcpy(p->name, "I CANT TYPE", 11);
                p->mod = 11;
            }
            p->state = need_spawn;
            break;
        }
    } while(consumed != length);

    return consumed;
}

void send_head(struct player* belongs_to, ssize_t ndx, int is_red) {
    char c1 = belongs_to->name[belongs_to->name_index++ % belongs_to->mod];
    //char c2 = belongs_to->name[belongs_to->name_index++ % belongs_to->mod];
    char c2 = ' ';

    FOR_PLAYER(p) {
        if(p->state != playing) continue;
        if(p == belongs_to) {
            SET_BACK(p->ns, ANSI_BACK_BLUE);
        } else if(!is_red) {
            SET_BACK(p->ns, ANSI_BACK_RED);
        }
        display_c(p->ns, ndx, c1, c2);
        if(p == belongs_to) {
            SET_BACK(p->ns, ANSI_BACK_RED);
        }
    }
}

void send_clear(ssize_t ndx, int is_black) {
    FOR_PLAYER(p) {
        if(p->state != playing) continue;
        if(!is_black) {
            SET_BACK(p->ns, ANSI_BACK_BLACK);
        }
        display_c(p->ns, ndx, ' ', ' ');
    }
}

void send_food(ssize_t ndx) {
    FOR_PLAYER(p) {
        if(p->state != playing) continue;
        SET_BACK(p->ns, ANSI_BACK_BLACK);
        display_c(p->ns, ndx, '@', '@');
    }
}

ssize_t next_head(ssize_t head, ssize_t dx, ssize_t dy) {
    ssize_t nx = ((X_I(head) + dx) + BWIDTH) % BWIDTH;
    ssize_t ny = ((Y_I(head) + dy) + BHEIGHT) % BHEIGHT;
    ssize_t next = INDX(nx, ny);
    return next;
}


void spawn_player(struct player* player, board_t* board, ssize_t food) {

    assert(player->state == need_spawn);
    ssize_t head, tail;

    player->diff = (diff_t){.dx = 1, .dy = 0};
    tail = (syscall_now() % BSIZE);
    do {
        tail = (tail + 1) % BSIZE;
        head = next_head(tail, 1, 0);
    } while (board[head] != EMPTY_SLOT || board[tail] != EMPTY_SLOT);

    player->head = head;
    player->tail = tail;
    player->diff = (diff_t){.dx = 1, .dy = 0};
    player->state = playing;
    player->name_index = 0;
    // draw the board
    display_board(player->ns, board, food);

    // send the new pieces to everyone

    board[player->tail] = (board_t)head;
    board[player->head] = HEAD_SLOT;

    // also send this out to everyone

    send_head(player, head, 0);
    send_head(player, tail, 0);

    printf("Spawn player!\n");
}

struct player* alloc_player(NET_SOCK ns) {
    struct player* player;
    player = &players[free_hd++];

    player->ns = ns;
    ns->sock.flags &= ~MSG_DONT_WAIT;
    player->state = getting_name;
    player->mod = 0;
    player->head = EMPTY_SLOT;
    printf("New player!\n");


    fprintf((FILE*)player->ns, "Welcome to CHERI-SNAKE! Type a name and hit enter. Use wasd to control.\n");

    return player;
}

void kill_players(board_t* board) {
    int is_black = 0;
    for(size_t i = 0; i < free_hd; i++) {
        if(players[i].state == need_kill) {
            printf("Player died\n");
            fprintf((FILE*)players[i].ns, "DIE!\n");

            if(players[i].head != EMPTY_SLOT) {
                ssize_t tail = players[i].tail;

                do {
                    send_clear(tail, is_black);
                    is_black = 1;
                    ssize_t next_tail = board[tail];
                    board[tail] = EMPTY_SLOT;
                    tail = next_tail;
                } while(tail != HEAD_SLOT);
            }
            close((FILE_t)players[i].ns);
            players[i] = players[free_hd-1];
            free_hd--;
            i--;
        }
    }
}



int main(__unused register_t arg, __unused capability carg) {

    // Set up a TCP server

    struct tcp_bind bind;
    bind.addr.addr = IP_ADDR_ANY->addr;
    bind.port = SNAKE_PORT;
    listening_token_or_er_t token_or_er = netsock_listen_tcp(&bind, 1, NULL, NULL);

    assert(IS_VALID(token_or_er));

    __unused listening_token tok = token_or_er.val;

    // Make the board
    board_t board[BSIZE];

    // Empty at start
    memset(board, EMPTY_SLOT, sizeof(board));

    // Just put the food somewhere
    ssize_t food = 7;


    while(1) {

        if(free_hd != 0) {
            // Handle inputs
            FOR_PLAYER(p) {
                ssize_t res = 0;
                if(p->state == playing || p->state == getting_name) {

                    res = socket_fulfill_progress_bytes_unauthorised(p->ns->sock.read.push_reader, SOCK_INF, F_PROGRESS | F_CHECK | F_DONT_WAIT,
                                                                             p->state == playing ? &TRUSTED_CROSS_DOMAIN(snake_ful) : &TRUSTED_CROSS_DOMAIN(snake_getname)
                                                                             , p, 0, NULL, NULL, TRUSTED_DATA,
                                                                             NULL);
                    if(res == E_AGAIN) res = 0;
                    if(res < 0) p->state = need_kill;
                }
            }

            // Spawn players

            FOR_PLAYER(p) {
                if(p->state == need_spawn) {
                    spawn_player(p, board, food);
                }
            }


            // Move snakes (biased but who cares)
            int is_black = 0;
            FOR_PLAYER(p) {
                if(p->state != playing) continue;

                ssize_t head = p->head;
                ssize_t tail = p->tail;
                ssize_t next = next_head(head, p->diff.dx, p->diff.dy);

                int got_food = next == food;
                int collide = board[next] != EMPTY_SLOT;

                if(collide) {
                    p->state = need_kill;
                    continue;
                }

                // update tail
                if(!got_food) {
                    ssize_t next_tail = board[tail];
                    board[tail] = EMPTY_SLOT;
                    send_clear(tail, is_black);
                    is_black = 1;
                    tail = next_tail;
                }

                // update head (skip drawing)
                board[head] = (board_t)next;
                head = next;
                board[head] = HEAD_SLOT;

                if(got_food) {
                    send_clear(food, is_black);
                    is_black = 1;
                    food = EMPTY_SLOT;
                }

                p->head = head;
                p->tail = tail;
            }

            // Draw in new heads (moved out of loop for less control characters)
            int is_red = 0;
            FOR_PLAYER(p) {
                if(p->state != playing) continue;
                send_head(p, p->head, is_red);
                is_red = 1;
            }

            if(food == EMPTY_SLOT) {
                food = syscall_now() % BSIZE;
                while(board[food] != EMPTY_SLOT)
                    food = (food + 1) % BSIZE;
                send_food(food);
            }

            kill_players(board);

            // Flush
            FOR_PLAYER(p) {
                if(p->state == playing) {
                    fprintf((FILE*)p->ns, ANSI_ESC_C "%d;%d" ANSI_SET_CURSOR, BHEIGHT + 3, 1);
                    socket_flush_drb(&p->ns->sock);
                }
            }

            // wait a bit
            sleep(GAME_SPEED);
        }

        // Accept new players
        if(free_hd != MAX_PLAYERS) {
            NET_SOCK ns;
            do {
                ns = netsock_accept(free_hd == 0 ? 0 : MSG_DONT_WAIT);
                if(ns) alloc_player(ns);
            } while(ns && free_hd != MAX_PLAYERS);
        }

    }
}
