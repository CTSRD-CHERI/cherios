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

#include "ff.h"
#include "misc.h"
#include "virtioblk.h"
#include "stdio.h"
#include "sockets.h"
#include "thread.h"
#include "stdlib.h"
#include "atomic.h"
#include "unistd.h"

FATFS fs;

#define MAX_HANDLES 0x80

struct sessions_t {
    unix_like_socket sock;
    FIL fil;
    size_t ndx;
    uint64_t read_fptr;
    uint64_t write_fptr;
    uint8_t current;
} sessions[MAX_HANDLES];

size_t first_free = 0;

poll_sock_t poll_socks[MAX_HANDLES];
size_t session_ndx[MAX_HANDLES];
size_t n_files = 0;

#define N_WORKERS 1
#define JOB_MAX 3

struct {
    thread t;
    act_kt message;
    volatile uint8_t jobs_assigned;
    volatile uint8_t jobs_done;
} workers[N_WORKERS];


__thread struct requester_32 sr_read;
__thread struct requester_32 sr_write;
__thread size_t r_socket_sector;
__thread size_t w_socket_sector;

act_notify_kt main_notify;

ssize_t full_oob(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length) {
    struct sessions_t* fil = (struct sessions_t*)arg;
    request_type_e req = request->type;

    if(req == REQUEST_SEEK) {

        uint64_t* modify = fil->current == 0 ? &fil->read_fptr : &fil->write_fptr;

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
            default:
                return E_OOB;
        }

        *modify = target_offset;

        return length;
    } else if(req == REQUEST_FLUSH) {
        assert(0 && "TODO");
    }

    return E_OOB;
}

void handle(enum poll_events events, struct sessions_t* session) {
    FRESULT fresult = 0;
    ssize_t res;
    UINT bytes_handled = 0;
    UINT bytes_to_push;

    if(events & POLL_IN) {
        // service write (we read)
        uni_dir_socket_fulfiller* read_fulfill = &session->sock.read.push_reader;
        uint64_t btp = socket_internal_requester_bytes_requested(read_fulfill->requester);
        do {

            bytes_to_push = (btp > UINT_MAX) ? (UINT) UINT_MAX : (UINT)btp;
            // This will handle all oob requests before any data
            session->current = 1;
            res = socket_internal_fulfill_progress_bytes(read_fulfill, SOCK_INF,
                                                                 1, 1, 1, 0,
                                                                 &ful_func_cancel_non_oob, (capability)session, 0, full_oob);
            if(session->fil.fptr != session->write_fptr) {
                f_lseek(&session->fil, session->write_fptr);
            }
            fresult = f_write(&session->fil, read_fulfill, (UINT)bytes_to_push, &bytes_handled);
            btp -= bytes_handled;
            session->write_fptr+=bytes_handled;
        } while(fresult == 0 && btp > 0 && bytes_handled != 0);
    }
    if(events & POLL_OUT) {
        // service read (we write)
        uni_dir_socket_fulfiller* write_fulfill = &session->sock.write.pull_writer;
        uint64_t btp = socket_internal_requester_bytes_requested(write_fulfill->requester);
        do {

            bytes_to_push = (btp > UINT_MAX) ? (UINT) UINT_MAX : (UINT)btp;
            // This will handle all oob requests before any data
            session->current = 0;
            res = socket_internal_fulfill_progress_bytes(write_fulfill, SOCK_INF,
                                                         1, 1, 1, 0,
                                                         &ful_func_cancel_non_oob, (capability)session, 0, full_oob);
            if(session->fil.fptr != session->read_fptr) {
                f_lseek(&session->fil, session->read_fptr);
            }
            fresult = f_read(&session->fil, write_fulfill, (UINT)bytes_to_push, &bytes_handled);
            btp -= bytes_handled;
            session->read_fptr +=bytes_handled;
        } while(fresult == 0 && btp > 0 && bytes_handled != 0);
    }


    // FIXME: Race between this and closing. This happens because poll has a bad interface.
    poll_socks[session->ndx].events = POLL_IN | POLL_OUT;
}

void worker_loop(register_t r, capability c) {

    ssize_t ret;

    ret = socket_internal_requester_init(&sr_read.r, 32, SOCK_TYPE_PULL, NULL);
    assert_int_ex(ret, ==, 0);
    ret = socket_internal_requester_init(&sr_write.r, 32, SOCK_TYPE_PUSH, NULL);
    assert_int_ex(ret, ==, 0);
    ret = virtio_new_socket(&sr_read.r, CONNECT_PULL_READ);
    assert_int_ex(ret, ==, 0);
    ret = virtio_new_socket(&sr_write.r, CONNECT_PUSH_WRITE);
    assert_int_ex(ret, ==, 0);

    w_socket_sector = 0;
    r_socket_sector = 0;

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
        syscall_cond_wait(0);
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


int new_file(uni_dir_socket_requester* read_requester, uni_dir_socket_requester* write_requester, const char* file_name) {

    if(!read_requester && !write_requester) return -1;

    if(first_free == MAX_HANDLES) {
        return -1;
    }

    size_t next = sessions[first_free].ndx;
    struct sessions_t* session = &sessions[first_free];

    FIL * fp = &session->fil;

    BYTE mode = FA_OPEN_ALWAYS;

    enum socket_connect_type con_type = CONNECT_NONE;
    enum poll_events events = POLL_NONE;

    if(write_requester) {
        con_type |= CONNECT_PUSH_READ;
        mode |= FA_WRITE;
        socket_internal_fulfiller_init(&session->sock.read.push_reader, SOCK_TYPE_PUSH);
        socket_internal_fulfiller_connect(&session->sock.read.push_reader, write_requester);
        events |= POLL_OUT;
    }
    if(read_requester) {
        con_type |= CONNECT_PULL_WRITE;
        mode |= FA_READ;
        socket_internal_fulfiller_init(&session->sock.write.pull_writer, SOCK_TYPE_PULL);
        socket_internal_fulfiller_connect(&session->sock.write.pull_writer, read_requester);
        events |= POLL_IN;
    }

    poll_socks[n_files].events = events;
    poll_socks[n_files].sock = &session->sock;
    session_ndx[n_files] = first_free;
    session->ndx = n_files;
    session->read_fptr = session->write_fptr = 0;

    if(socket_init(&session->sock, MSG_DONT_WAIT | MSG_NO_CAPS, NULL, 0, con_type) == 0) {
        if(f_open(fp, file_name, mode) == 0) {
            n_files++;
            first_free = next;
            return 0;
        }
    }

    return -1;
}

void close_file(size_t sndx, struct sessions_t* session) {
    // First close the unix socket
    socket_close(&session->sock);

    // Then close the file
    f_close(&session->fil);

    // Then compact poll_socks
    size_t poll_ndx = session->ndx;
    size_t poll_to_move = n_files-1;

    if(poll_ndx != poll_to_move) {
        size_t session_ndx_to_move = session_ndx[poll_to_move];
        poll_socks[poll_ndx] = poll_socks[poll_to_move];
        session_ndx[poll_ndx] = session_ndx_to_move;
        sessions[session_ndx_to_move].ndx = poll_ndx;
    }

    n_files--;

    // Then free struct
    session->ndx = first_free;
    first_free = sndx;
}

void (*msg_methods[]) = {new_file};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

void request_loop(void) {

    enum poll_events msg_event;

    for(size_t i = 0; i != MAX_HANDLES; i++) {
        sessions[i].ndx = i+1;
    }


    while(1) {
        size_t poll_to = n_files;
        socket_poll(poll_socks, poll_to, &msg_event);

        if(msg_event) {
            msg_entry(1);
        }

        for(size_t i = 0; i < poll_to; i++) {
            poll_sock_t* poll_sock = poll_socks+i;
            size_t sndx = session_ndx[i];
            struct sessions_t* session = &sessions[sndx];
            if(poll_sock->revents & (POLL_IN | POLL_OUT)) {
                assert_int_ex(poll_sock->events, !=, POLL_NONE);
                poll_sock->events = POLL_NONE;
                assign_work(poll_sock->revents, session);
            } else {
                if(poll_sock->revents & POLL_HUP) {
                    // a close happened
                    close_file(sndx, session);
                    continue; // closing may move things around. re-poll
                }
                if(poll_sock->revents & POLL_ER) {
                    assert(0 && "TODO");
                }
            }
        }
    }
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
        if ((res = f_mkfs("", 0, 0))) {
            printf("MK:%d\n", res);
            goto er;
        }
    } else {
        already_existed = 1;
    }

    spawn_workers();

    namespace_register(namespace_num_fs, act_self_ref);

    printf("Fatfs: Going into daemon mode\n");

    request_loop();

    er:
    printf("Fatfs: Failed to start");

    return -1;
}
