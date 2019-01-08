/*-
 * Copyright (c) 2018 Lawrence Esswood
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

#include <sockets.h>
#include "cheric.h"
#include "assert.h"
#include "nano/nanokernel.h"
#include "virtioblk.h"
#include "thread.h"
#include "misc.h"
#include "stdlib.h"
#include "ringbuffers.h"
#include "lists.h"
// TODO currently this never flushes blocks back =(

#define BLOCK_BITS  20

// The sector size might be a lot smaller, but we might as well cache at this granularity
#define BLOCK_SIZE  (1 << BLOCK_BITS)
// TODO get this from the driver
#define SECTOR_SIZE 512
#define MAX_SOCKS 4

typedef struct block_cache_ent_t {
    char data[BLOCK_SIZE];
    int is_complete;
    //size_t references;
    //size_t reuses; // Make sure we eventually release a block. Set to 0 if we ever release a ptr to this block
} block_cache_ent_t;

enum session_state {
    created,
    initted,
    destroyed,
};

typedef struct session_sock {
    struct session_t* session;
    uni_dir_socket_fulfiller ff;
    size_t addr;
    block_cache_ent_t* blocked;
} session_sock;

typedef struct {
    size_t sector;
    int is_write;
    sync_state_t sync_ret;
    char* buf;
} io_callback_t;



typedef struct session_t {
#define RB_CB(...) RINGBUF_MEMBER_STATIC(callbacks, io_callback_t, uint8_t, 0x20, __VA_ARGS__)
#define RB_RD(...) RINGBUF_MEMBER_STATIC(blockreads, size_t, uint8_t, 0x20, __VA_ARGS__)
    capability block_session;
    size_t size;
    enum session_state state;
    uint8_t should_poll;
    RINGBUF_DEF_FIELDS_STATIC(RB_CB);
    RINGBUF_DEF_FIELDS_STATIC(RB_RD);
    RINGBUF_DEF_BUF_STATIC(RB_CB);
    RINGBUF_DEF_BUF_STATIC(RB_RD);
    volatile uint64_t blockreads_ack;
    struct requester_32 read_req;
    // TODO a write requester
    DLL_LINK(session_t);
    block_cache_ent_t** block_cache;
} session_t;

struct {
    DLL(session_t);
} session_list;

capability sealer;
session_sock socks[MAX_SOCKS];
size_t socks_count;

static session_t* seal_session(session_t* session) {
    return (session_t*)cheri_seal(session, sealer);
}

static session_t* unseal_session(session_t* session) {
    session = cheri_unseal_2(session, sealer);
    if(session == NULL) return NULL;
    enum session_state state;
    ENUM_VMEM_SAFE_DEREFERENCE(&session->state, state, destroyed);
    return (state == destroyed) ? NULL : session;
}

static session_t* new_session(void* mmio_cap) {
    capability block_session = message_send_c(0, 0 ,0, 0, mmio_cap, NULL, NULL, NULL, vblk_ref, SYNC_CALL, -1);
    if(block_session == NULL) return NULL;

    session_t* session = (session_t*)malloc(sizeof(session_t));
    session->block_session = block_session;
    session->state = created;

    DLL_ADD_END(&session_list, session);
    return seal_session(session);
}

static int vblk_init(session_t* session) {
    session = unseal_session(session);

    assert(session != NULL);
    assert(session->state == created);

    // init
    int ret = (int)message_send(0, 0, 0, 0, session->block_session, NULL, NULL, NULL, vblk_ref, SYNC_CALL, 0);
    if(ret != 0) return ret;

    // hook up a requester
    uni_dir_socket_requester* r = &session->read_req.r;
    socket_internal_requester_init(r,32,SOCK_TYPE_PULL,NULL);
    r->drb_fulfill_ptr = &session->blockreads_ack;
    ret  = (int)message_send(SOCK_TYPE_PULL, 0, 0, 0, session->block_session, r, NULL, NULL, vblk_ref, SYNC_CALL, 5);
    if(ret != 0) return ret;
    socket_internal_requester_connect(r);

    // get size and allocate the cache map
    size_t size = (size_t)message_send(0, 0, 0, 0, session->block_session, NULL, NULL, NULL, vblk_ref, SYNC_CALL, 4) * SECTOR_SIZE;
    size_t nblocks = (size + (BLOCK_SIZE-1)) >> BLOCK_BITS;
    session->block_cache = (block_cache_ent_t**)malloc(nblocks * sizeof(block_cache_ent_t*));
    session->size = size;

    // finish
    session->state = initted;

    return 0;
}

static int new_socket(session_t* session, uni_dir_socket_requester* requester, enum socket_connect_type type) {
    session = unseal_session(session);

    assert(session != NULL);
    assert(session->state == initted);

    if(socks_count == MAX_SOCKS) return -1;

    session_sock* ss = &socks[socks_count];

    uint8_t sock_type;
    if(type == CONNECT_PUSH_WRITE) {
        sock_type = SOCK_TYPE_PUSH;
    } else if(type == CONNECT_PULL_READ) {
        sock_type = SOCK_TYPE_PULL;
    } else return -1;

    ssize_t res;
    if((res = socket_internal_fulfiller_init(&ss->ff, sock_type)) < 0) return (int)res;
    if((res = socket_internal_fulfiller_connect(&ss->ff, requester)) < 0) return (int)res;

    ss->session = session;
    ss->addr = 0;

    socks_count++;

    return 0;
}

static size_t vblk_size(session_t* session) {
    session = unseal_session(session);
    assert(session != NULL);
    assert(session->state == initted);
    return session->size / SECTOR_SIZE;
}

static int new_block(session_t* session, size_t index) {
    // Create a new block
    block_cache_ent_t* block = (block_cache_ent_t*)malloc(sizeof(block_cache_ent_t));
    session->block_cache[index] = block;

    // Seek and indirect read
    uni_dir_socket_requester* r = &session->read_req.r;
    ssize_t res = socket_internal_requester_space_wait(r,2,0,0);
    assert(res == 0);

    size_t sector_start = (index << BLOCK_BITS) / SECTOR_SIZE;

    seek_desc sk;

    sk.v.whence = SEEK_SET;
    sk.v.offset = sector_start;

    *RINGBUF_PUSH(RB_RD, session) = index;
    res = socket_internal_request_oob(r,REQUEST_SEEK,sk.as_intptr_t,0,0);
    assert(res == 0);
    res = socket_internal_request_ind(r,block->data,BLOCK_SIZE,1);
    assert(res == 0);

    session->should_poll = 1;
    return 0;
}

static inline void cpy(char*buf, char* sector_buf, int is_write, size_t to_copy) {
    char* src = is_write ? buf : sector_buf;
    char* dst = is_write ? sector_buf : buf;
    memcpy(dst,src,to_copy);
}

static void handle_callbacks(session_t* session) {
    while(RINGBUF_HD(RB_RD,session) != (RINGBUF_INDEX_T(RB_RD))session->blockreads_ack) {
        assert(!RINGBUF_EMPTY(RB_RD, session));
        size_t map_index = *RINGBUF_POP(RB_RD, session);
        session->block_cache[map_index]->is_complete = 1;
    }

    if(RINGBUF_EMPTY(RB_RD, session)) session->should_poll = 0;

    RINGBUF_FOREACH_POP(cb,RB_CB,session) {
        size_t map_index = (cb->sector * SECTOR_SIZE) >> BLOCK_BITS;
        size_t map_offset = (cb->sector * SECTOR_SIZE) & (BLOCK_SIZE-1);

        if(!session->block_cache[map_index]->is_complete) break;

        char* sector_buf = session->block_cache[map_index]->data + map_offset;
        cpy(cb->buf, sector_buf, cb->is_write, SECTOR_SIZE);

        msg_resume_return(NULL,0,0,cb->sync_ret);
    }

}

static int rw_sector(session_t* session, size_t sector, char* buf, int is_write) {
    session = unseal_session(session);
    assert(session != NULL);
    assert(session->state == initted);
    size_t map_index = (sector * SECTOR_SIZE) >> BLOCK_BITS;
    size_t map_offset = (sector * SECTOR_SIZE) & (BLOCK_SIZE-1);
    block_cache_ent_t* ent = session->block_cache[map_index];
    if(ent == NULL || !ent->is_complete) {
        // allocate a sync callback
        if(ent == NULL) new_block(session, map_index);

        io_callback_t* cb = RINGBUF_PUSH(RB_CB,session);

        if(cb == NULL) return -1;

        cb->sector = sector;
        msg_delay_return(&cb->sync_ret);
        cb->buf = buf;
        cb->is_write = is_write;
    } else {
        char* sector_buf = ent->data + map_offset;
        cpy(buf,sector_buf,is_write, SECTOR_SIZE);
    }

    return 0;
}


static ssize_t ff(capability arg, char* buf, uint64_t offset, uint64_t length) {
    session_sock* ss = (session_sock*)arg;

    size_t addr = ss->addr;
    size_t copied = 0;

    size_t map_index;
    size_t map_offset;

    while(length != 0) {
        map_index = (addr) >> BLOCK_BITS;
        map_offset = (addr) & (BLOCK_SIZE-1);
        if((!ss->session->block_cache[map_index] && new_block(ss->session,map_index))
           || !ss->session->block_cache[map_index]->is_complete) {
            ss->blocked = ss->session->block_cache[map_index];
            break;
        }

        size_t biggest_copy = BLOCK_SIZE - map_offset;
        size_t to_copy = length > biggest_copy ? biggest_copy : length;

        char* block_buf = ss->session->block_cache[map_index]->data + map_offset;

        cpy(buf,block_buf,ss->ff.socket_type == SOCK_TYPE_PUSH, to_copy);

        copied +=to_copy;
        addr +=to_copy;
        length -=to_copy;
    }

    ss->addr = addr;

    return copied;
}

static ssize_t ff_sub(capability arg, uint64_t offset, uint64_t length, char** out_buf) {
    session_sock* ss = (session_sock*)arg;

    size_t addr = ss->addr;

    size_t map_index = (addr) >> BLOCK_BITS;
    size_t map_offset = (addr) & (BLOCK_SIZE-1);

    if((!ss->session->block_cache[map_index] && new_block(ss->session,map_index))
       || !ss->session->block_cache[map_index]->is_complete) {
        ss->blocked = ss->session->block_cache[map_index];
        return 0;
    }

    char* block_buf = ss->session->block_cache[map_index]->data + map_offset;

    size_t biggest_copy = BLOCK_SIZE - map_offset;
    size_t to_copy = length > biggest_copy ? biggest_copy : length;

    // TODO at this point we have to somehow track when the resulting request is fulfilled. Then we know we can release the buffer

    *out_buf = cheri_setbounds_exact(block_buf, to_copy);

    ss->addr = addr + to_copy;

    return to_copy;
}

static ssize_t oobff(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length) {
    session_sock* ss = (session_sock*)arg;
    request_type_e req = request->type;

    if(req == REQUEST_SEEK) {
        assert_int_ex(ss->addr & (SECTOR_SIZE-1), ==, 0);
        int64_t seek_offset = request->request.seek_desc.v.offset;
        int whence = request->request.seek_desc.v.whence;

        size_t target_offset;

        switch (whence) {
            case SEEK_CUR:
                target_offset = (seek_offset * SECTOR_SIZE) + ss->addr;
                break;
            case SEEK_SET:
                if(seek_offset < 0) return E_OOB;
                target_offset = (size_t)(seek_offset * SECTOR_SIZE);
                break;
            case SEEK_END:
            default:
                return E_OOB;
        }

        ss->addr = target_offset;

        return length;
    } else if(req == REQUEST_FLUSH) {
        assert(0 && "TODO");
    }

    return E_OOB;
}


static void handle_sock_session(session_sock* ss) {
    ssize_t res = socket_internal_fulfill_progress_bytes(&ss->ff, SOCK_INF, F_CHECK | F_DONT_WAIT | F_PROGRESS,
                                                         &ff, (capability)ss, 0, &oobff, ff_sub);
    if(res == E_AGAIN) return;

    assert_int_ex(res, >=, 0);

}

static int vblk_read(session_t* session, void * buf, size_t sector) {
    return rw_sector(session, sector, buf, 0);
}

static int vblk_write(session_t* session, void * buf, size_t sector) {
    return rw_sector(session, sector, buf, 1);
}

static uint8_t vblk_status(session_t* session) {
    session = unseal_session(session);
    return (uint8_t)message_send(0, 0, 0, 0, session->block_session, NULL, NULL, NULL, vblk_ref, SYNC_CALL, 3);
}

static void main_loop(void) {
    POLL_LOOP_START(sleep, any_event, 1)
        DLL_FOREACH(session_t, s, &session_list) {
            handle_callbacks(s);
        }
        for(size_t i = 0; i != socks_count; i++) {
            session_sock* ss = &socks[i];

            if(ss->blocked && ss->blocked->is_complete) ss->blocked = NULL;

            if(!ss->blocked) {
                // Poll the socket when new incoming reads come in
                POLL_ITEM_F(event,sleep,any_event,&ss->ff, POLL_IN,0);
                if(event) {
                    if(event & POLL_IN) {
                        handle_sock_session(ss);
                    } else {
                        assert(0); // No errors allowed for now
                    }
                }
            }
        }
        DLL_FOREACH(session_t, s, &session_list) {
            // Poll for outgoing reads finishing
            if(s->should_poll) {
                POLL_ITEM_R(event, sleep, any_event, &s->read_req.r, POLL_OUT, 32);
                if(event) {
                    handle_callbacks(s);
                }
            }
        }
    POLL_LOOP_END(sleep, any_event, 1, 0);
}

int main(register_t arg, capability carg) {
    while((vblk_ref = namespace_get_ref(namespace_num_virtio)) == NULL) {
        sleep(0);
    }
    sealer = get_type_owned_by_process();

    int res = namespace_register(namespace_num_blockcache,act_self_ref);

    assert(res == 0);

    main_loop();
}

void (*msg_methods[]) = {vblk_init, vblk_read, vblk_write, vblk_status, vblk_size, new_socket};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, new_session, NULL, NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);