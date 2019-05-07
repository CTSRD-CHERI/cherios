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

#include "sockets.h"
#include <ff.h>
#include "spinlock.h"
#include "ff.h"
#include "misc.h"
#include "virtioblk.h"
#include "stdio.h"
#include "thread.h"
#include "stdlib.h"
#include "atomic.h"
#include "unistd.h"

FATFS fs;

#define MAX_HANDLES 0x80

struct sessions_t {
    unix_like_socket sock;
    FIL fil;
    locked_t encrypt_lock;
    size_t next_ndx; // either next free, or next in list of open files
    uint64_t read_fptr;
    uint64_t write_fptr;
    enum poll_events events;
    uint8_t current;
    spinlock_t session_lock;
    uint8_t in_use;
    uint8_t nice_close;
} sessions[MAX_HANDLES];

size_t first_free = 0;
size_t first_file = MAX_HANDLES;
size_t n_files = 0;

#define N_WORKERS 1
#define JOB_MAX 3

struct {
    thread t;
    act_kt message;
    volatile uint8_t jobs_assigned;
    volatile uint8_t jobs_done;
} workers[N_WORKERS];


__thread fs_proxy sr_read;
__thread fs_proxy sr_write;

act_notify_kt main_notify;

static void set_encrypt_lock(fs_proxy* proxy, locked_t locked) {
    if(proxy->encrypt_lock != locked) {
        int res = socket_requester_space_wait(proxy->requester, 1, 0, 0);
        assert_int_ex(res, ==, 0);
        res = socket_request_oob(proxy->requester, REQUEST_SET_KEY, (intptr_t)locked, 0, 0);
        assert_int_ex(res, ==, 0);
        proxy->encrypt_lock = locked;
    }
}


void close_file(size_t* prev_ndx, struct sessions_t* session, uint8_t level) {
    assert_int_ex(session->in_use, ==, 1);

    size_t poll_ndx;
    size_t poll_to_move;

    if(session->nice_close < level) {
        if(level > 4) level = 4;
        switch(session->nice_close) {
            case 0:
                // Close the underlying file
                f_close(&session->fil);
                if(level == 1) break;
            case 1:
                // Close the socket
                socket_close(&session->sock);
                if(level == 2) break;
            case 2:
                // Used to have a poll socks structure. Now no more!
                n_files--;
                if(level == 3) break;
            case 3:
                // Then free struct
                session->in_use = 0;
                size_t this_ndx = *prev_ndx;
                *prev_ndx = session->next_ndx;
                session->next_ndx = first_free;
                first_free = this_ndx;

        }
        session->nice_close = level;
    }
}

ssize_t TRUSTED_CROSS_DOMAIN(full_oob)(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length);
ssize_t full_oob(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length) {
    struct sessions_t* fil = (struct sessions_t*)arg;
    assert(!fil->nice_close);
    request_type_e req = request->type;
    uint64_t* modify;
    ssize_t * size_ptr;
    switch(req) {
        case REQUEST_SEEK:
            assert(!fil->encrypt_lock); // currently don't support seek here
            modify = fil->current == 0 ? &fil->read_fptr : &fil->write_fptr;

            int64_t seek_offset = request->request.seek_desc.v.offset;
            int whence = request->request.seek_desc.v.whence;

            FSIZE_t target_offset;

            switch (whence) {
                case SEEK_CUR:
                    target_offset = seek_offset + *modify;
                    break;
                case SEEK_SET:
                    if(seek_offset < 0) return E_OOB;
                    target_offset = (uint64_t)seek_offset;
                    break;
                case SEEK_END:
                    target_offset = (seek_offset + fil->fil.fptr);
                    break;
                default:
                    return E_OOB;
            }

            *modify = target_offset;

            return length;
        case REQUEST_SIZE:
            size_ptr = (size_t*)request->request.oob;
            *size_ptr = f_size(&fil->fil);

            return length;
        case REQUEST_TRUNCATE:
            f_truncate(&fil->fil);
            return length;
        case REQUEST_CLOSE:
            close_file(NULL, fil, 1);
            return length;
        case REQUEST_FLUSH:
            assert(0 && "TODO");
        default:
            assert(0 && "UNKNOWN OOB");
            return E_OOB;
    }

}



void handle(enum poll_events events, struct sessions_t* session) {
    FRESULT fresult = 0;
    ssize_t res;
    UINT bytes_handled = 0;
    UINT bytes_to_push;

    assert_int_ex(session->in_use, ==, 1);

    assert(!session->nice_close);
    assert(session->fil.obj.fs);

    if(events & POLL_IN) {
        // service write (we read)
        fulfiller_t read_fulfill = session->sock.read.push_reader;
        assert_int_ex(in_proxy(read_fulfill), ==, 0);
        uint64_t btp = socket_fulfiller_bytes_requested(read_fulfill);
        do {

            bytes_to_push = (btp > UINT_MAX) ? (UINT) UINT_MAX : (UINT)btp;
            // This will handle all oob requests before any data
            session->current = 1;
            res = socket_fulfill_progress_bytes_unauthorised(read_fulfill, SOCK_INF,
                                                                 F_CHECK | F_PROGRESS | F_DONT_WAIT | F_CANCEL_NON_OOB,
                                                                 NULL,
                                                                 (capability)session, 0, TRUSTED_CROSS_DOMAIN(full_oob),
                                                                 NULL, NULL, TRUSTED_DATA);
            if(res == E_SOCKET_CLOSED) break;
            assert(res >= 0 || (res == E_AGAIN));
            if(session->fil.fptr != session->write_fptr) {
                f_lseek(&session->fil, session->write_fptr);
            }
            set_encrypt_lock(&sr_write, session->encrypt_lock);
            fresult = f_write(&session->fil, read_fulfill, (UINT)bytes_to_push, &bytes_handled);
            btp -= bytes_handled;
            session->write_fptr+=bytes_handled;
        } while(fresult == 0 && btp > 0 && bytes_handled != 0);
    }
    if(events & POLL_OUT) {
        // service read (we write)
        fulfiller_t write_fulfill = session->sock.write.pull_writer;
        assert_int_ex(in_proxy(write_fulfill), ==, 0);
        uint64_t btp = socket_fulfiller_bytes_requested(write_fulfill);
        do {

            bytes_to_push = (btp > UINT_MAX) ? (UINT) UINT_MAX : (UINT)btp;
            // This will handle all oob requests before any data
            session->current = 0;
            res = socket_fulfill_progress_bytes_unauthorised(write_fulfill, SOCK_INF,
                                                         F_CHECK | F_PROGRESS | F_DONT_WAIT | F_CANCEL_NON_OOB,
                                                         NULL,
                                                         (capability)session, 0, TRUSTED_CROSS_DOMAIN(full_oob),
                                                         NULL,NULL, TRUSTED_DATA);
            if(res == E_SOCKET_CLOSED) break;
            assert(res >= 0 || (res == E_AGAIN));
            if(session->fil.fptr != session->read_fptr) {
                f_lseek(&session->fil, session->read_fptr);
            }
            set_encrypt_lock(&sr_read, session->encrypt_lock);
            fresult = f_read(&session->fil, write_fulfill, (UINT)bytes_to_push, &bytes_handled);
            btp -= bytes_handled;
            session->read_fptr +=bytes_handled;
        } while(fresult == 0 && btp > 0 && bytes_handled != 0);
    }

    if(session->nice_close) return;

    enum poll_events wait_for = POLL_NONE;
    if(session->sock.con_type & CONNECT_PUSH_READ) wait_for |= POLL_IN;
    if(session->sock.con_type & CONNECT_PULL_WRITE) wait_for |= POLL_OUT;

    session->events = wait_for;
}

void worker_loop(register_t r, capability c) {

    ssize_t ret;

    requester_t read = socket_malloc_requester_32(SOCK_TYPE_PULL, NULL);
    requester_t write = socket_malloc_requester_32(SOCK_TYPE_PUSH, NULL);

    assert(read != NULL && write != NULL);

    ret = virtio_new_socket(read, CONNECT_PULL_READ);
    assert_int_ex(ret, ==, 0);
    ret = virtio_new_socket(write, CONNECT_PUSH_WRITE);
    assert_int_ex(ret, ==, 0);

    sr_read.socket_sector = sr_write.socket_sector = sr_read.length = sr_write.length = 0;

    sr_read.requester = read;
    sr_write.requester = write;

    while(1) {
        msg_t *msg = get_message();
        handle((enum poll_events)msg->a0, (struct sessions_t*)msg->c3);
        next_msg();
        workers[r].jobs_done++;
        syscall_cond_notify(main_notify);
    }
}

void assign_work(enum poll_events events, struct sessions_t* session) {
    // TODO proper fan out
    size_t ndx = 0;

    while(workers[ndx].jobs_assigned - workers[ndx].jobs_done >= JOB_MAX) {
        syscall_cond_wait(0, 0);
    }

    message_send(events,0,0,0,(capability)session,NULL,NULL,NULL,workers[ndx].message, SEND, 0);
}


void spawn_workers(void) {
    main_notify = act_self_notify_ref;
    for(register_t i = 0; i < N_WORKERS;i++) {
        char name[] = "fatwrk_00";
        itoa((int)i, name+7,16);
        workers[i].jobs_done = workers[i].jobs_assigned = 0;
        workers[i].t = thread_new(name, i, act_self_ref, &worker_loop);
        workers[i].message = syscall_act_ctrl_get_ref(get_control_for_thread(workers[i].t));
    }
}


int open_file(int mode, requester_t read_requester, requester_t write_requester, const char* file_name, locked_t* encrpyt) {

    int read = mode & FA_READ;
    int write = mode & FA_WRITE;

    if(!read && !write) return FR_INVALID_PARAMETER;

    if(first_free == MAX_HANDLES) {
        return FR_TOO_MANY_OPEN_FILES;
    }

    struct sessions_t* session = &sessions[first_free];

    assert_int_ex(session->in_use, ==, 0);

    FIL * fp = &session->fil;

    enum socket_connect_type con_type = CONNECT_NONE;
    enum poll_events events = POLL_NONE;

    if(write) {
        assert(write_requester);
        con_type |= CONNECT_PUSH_READ;
        if(session->sock.read.push_reader) {
            int ret = socket_reuse_fulfiller(session->sock.read.push_reader, SOCK_TYPE_PUSH);
            assert_int_ex(ret, ==, 0);
        } else {
            session->sock.read.push_reader = socket_malloc_fulfiller(SOCK_TYPE_PUSH);
        }
        socket_fulfiller_connect(session->sock.read.push_reader, write_requester);
        events |= POLL_IN;
    }
    if(read) {
        assert(read_requester);
        con_type |= CONNECT_PULL_WRITE;
        mode |= FA_READ;
        if(session->sock.write.pull_writer) {
            int ret = socket_reuse_fulfiller(session->sock.write.pull_writer, SOCK_TYPE_PULL);
            assert_int_ex(ret, ==, 0);
        } else {
            session->sock.write.pull_writer = socket_malloc_fulfiller(SOCK_TYPE_PULL);
        }
        socket_fulfiller_connect(session->sock.write.pull_writer, read_requester);
        events |= POLL_OUT;
    }

    FRESULT fres = FR_INVALID_PARAMETER;
    if(socket_init(&session->sock, MSG_DONT_WAIT | MSG_NO_CAPS, NULL, 0, con_type) == 0) {
        if((fres = f_open(fp, file_name, (BYTE)mode)) == 0) {
            session->read_fptr = session->write_fptr = 0;
            session->encrypt_lock = encrpyt;
            spinlock_init(&session->session_lock);
            session->in_use = 1;
            session->nice_close = 0;
            size_t next = session->next_ndx;
            session->events = events;
            session->next_ndx = first_file;
            first_file = first_free;
            n_files++;
            first_free = next;
            return 0;
        }
    }

    return fres;
}

int make_dir(const char* name) {
    return f_mkdir(name);
}

capability dir_sealer;

static capability open_dir(const char* name) {
    DIR* dp = (DIR*)malloc(sizeof(DIR));

    FRESULT res = f_opendir(dp, name);

    if(res != FR_OK) {
        free(dp);
        return NULL;
    }

    return cheri_seal(dp, dir_sealer);
}

static FRESULT read_dir(capability sealed_dir, FILINFO* fno) {
    DIR* dp = cheri_unseal(sealed_dir, dir_sealer);
    return f_readdir(dp, fno);
}

static FRESULT close_dir(capability sealed_dir) {
    DIR* dp = cheri_unseal(sealed_dir, dir_sealer);
    FRESULT res = f_closedir(dp);
    free(dp);
    return res;
}

void (*msg_methods[]) = {open_file, make_dir, f_rename, f_unlink, f_stat, open_dir, read_dir, close_dir};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

void request_loop(void) {

    for(size_t i = 0; i != MAX_HANDLES; i++) {
        sessions[i].next_ndx = i+1;
    }

    POLL_LOOP_START(sock_sleep, sock_event, 1);

        restart_loop: {}

        size_t * prev_ptr = &first_file;
        for(size_t i = first_file; i!= MAX_HANDLES; i = sessions[i].next_ndx) {
            struct sessions_t* session = &sessions[i];
            assert_int_ex(session->in_use, ==, 1);

            if(session->nice_close == 2) {
                close_file(prev_ptr, session, UINT8_MAX);
                goto restart_loop;
            }

            if(session->events != POLL_NONE) {

                enum poll_events event = 0;

                if(session->events & POLL_IN) {
                    POLL_ITEM_F(event_rd, sock_sleep, sock_event, session->sock.read.push_reader, POLL_IN, 0);
                    event |= event_rd;
                }
                if(session->events & POLL_OUT) {
                    POLL_ITEM_F(event_wr, sock_sleep, sock_event, session->sock.write.pull_writer, POLL_OUT, 0);
                    event |= event_wr;
                }

                if(event & (POLL_IN | POLL_OUT)) {
                    session->events = POLL_NONE;
                    assign_work(event, session);
                } else if(event){
                    if(event & POLL_HUP) {
                        close_file(prev_ptr, session, UINT8_MAX);
                        goto restart_loop;
                    }
                    if(event & (POLL_ER | POLL_NVAL)) {
                        assert(0 && "TODO");
                    }
                }
            }

            prev_ptr = &session->next_ndx;
        }

    POLL_LOOP_END(sock_sleep, sock_event, 1, 0);
}

int main(capability fs_cap) {
    printf("Fatfs: Hello world\n");

    /* Init virtio-blk session */
    virtio_blk_session(fs_cap);

    FRESULT res;

    int already_existed;

    if ((res = f_mount(&fs, "", 1)) != FR_NO_FILESYSTEM && res != 0) {
        printf("MT:%d\n", res);
        goto er;
    }

    if (res == FR_NO_FILESYSTEM) {
        already_existed = 0;
        printf("Fatfs: Making FS. This may take some time. Hope you don't need that disk...\n");
        if ((res = f_mkfs("", 0, 0))) {
            printf("MK:%d\n", res);
            goto er;
        }
    } else {
        already_existed = 1;
    }

    if(!already_existed && (res = f_mount(&fs, "", 1))) {
        printf("MT(2):%d\n", res);
        goto er;
    }

    spawn_workers();

    namespace_register(namespace_num_fs, act_self_ref);

    dir_sealer = get_type_owned_by_process();

    assert(dir_sealer != NULL);

    printf("Fatfs: Going into daemon mode\n");

    request_loop();

    er:
    printf("Fatfs: Failed to start");

    return -1;
}
