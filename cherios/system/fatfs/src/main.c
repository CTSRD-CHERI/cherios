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

FATFS fs;

#define MAX_HANDLES 0x80

struct sessions_t {
    unix_like_socket sock;
    FIL fil;
};

struct sessions_t sessions[MAX_HANDLES];
poll_sock_t poll_socks[MAX_HANDLES];
size_t n_files = 0;

int new_file(uni_dir_socket_requester* read_requester, uni_dir_socket_requester* write_requester, const char* file_name) {

    if(!read_requester && !write_requester) return -1;

    struct sessions_t* session = sessions + n_files;

    FIL * fp = &session->fil;

    BYTE mode = FA_OPEN_EXISTING;

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

    if(socket_init(&session->sock, MSG_DONT_WAIT | MSG_NO_CAPS, NULL, 0, con_type) == 0) {
        if(f_open(fp, file_name, mode) == 0) {
            n_files++;
            return 0;
        }
    }

    return -1;
}

void (*msg_methods[]) = {new_file};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

//typedef ssize_t ful_func(capability arg, char* buf, uint64_t offset, uint64_t length);
//typedef ssize_t ful_oob_func(request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length);

ssize_t full_oob(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length) {
    FIL* fil = (FIL*)arg;
    request_type_e req = request->type;

    if(req == REQUEST_SEEK) {

        int64_t seek_offset = request->request.seek_desc.v.offset;
        int whence = request->request.seek_desc.v.whence;

        FSIZE_t target_offset;

        switch (whence) {
            case SEEK_CUR:
                target_offset = seek_offset + fil->fptr;
                break;
            case SEEK_SET:
                if(seek_offset < 0) return E_OOB;
                target_offset = (uint64_t)seek_offset;
                break;
            case SEEK_END:
            default:
                return E_OOB;
        }

        f_lseek(fil, target_offset);

        return length;
    }

    return E_OOB;
}

ssize_t ful_push_ff(capability arg, char* buf, uint64_t offset, uint64_t length) {
    FIL* fil = (FIL*)arg;
    UINT count;
    ssize_t total = 0;
    do {
        UINT len = length > ULONG_MAX ? (UINT)UINT_MAX : (UINT)length;
        FRESULT res =  f_write(fil, buf, len, &count);
        length -= count;
        buf+=count;
        total += count;
        if(res != 0) break;
    } while(length);
    return total;
}

ssize_t ful_pull_ff(capability arg, char* buf, uint64_t offset, uint64_t length) {
    FIL* fil = (FIL*)arg;
    UINT count;
    ssize_t total = 0;
    do {
        UINT len = length > ULONG_MAX ? (UINT)UINT_MAX : (UINT)length;
        FRESULT res = f_read(fil, buf, len, &count);
        length -= count;
        buf+=count;
        total += count;
        if(res != 0) break;
    } while(length);
    return total;
}

void request_loop(void) {

    enum poll_events msg_event;



    for(size_t i = 0; i != MAX_HANDLES; i++) {
        poll_socks[i].sock = &sessions[i].sock;
    }


    while(1) {
        socket_poll(poll_socks, n_files, &msg_event);

        if(msg_event) {
            msg_entry(1);
        }

        for(size_t i = 0; i != n_files; i++) {
            poll_sock_t* poll_sock = poll_socks+i;
            if(poll_sock->revents & POLL_IN) {
                // service write
                uni_dir_socket_fulfiller* read_fulfill = &poll_sock->sock->read.push_reader;
                ssize_t res = socket_internal_fulfill_progress_bytes(read_fulfill, SOCK_INF,
                                                       1, 1, 1, 0,
                                                       ful_push_ff, (capability)&sessions[i].fil, 0, full_oob);
            }
            if(poll_sock->revents & POLL_OUT) {
                // service read
                // service write
                uni_dir_socket_fulfiller* read_fulfill = &poll_sock->sock->write.pull_writer;
                ssize_t res = socket_internal_fulfill_progress_bytes(read_fulfill, SOCK_INF,
                                                                    1, 1, 1, 0,
                                                                    ful_pull_ff, (capability)&sessions[i].fil, 0, full_oob);
            }
            if(poll_sock->revents & POLL_HUP) {
                // a close happened
                assert(0 && "TODO");
            }
            if(poll_sock->revents & POLL_ER) {
                assert(0 && "TODO");
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

    namespace_register(namespace_num_fs, act_self_ref);

    printf("Fatfs: Going into daemon mode\n");

    request_loop();

    er:
    printf("Fatfs: Failed to start");

    return -1;
}
