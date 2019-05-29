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
#ifndef CHERIOS_SOCKETS_H
#define CHERIOS_SOCKETS_H

// FIXME
#include "../cherios/system/libsocket/include/socket_common.h"
#include "dylink.h"
#include "object.h"
#include "string.h"

PLT_thr(lib_socket_if_t, SOCKET_LIB_IF_LIST)
#define ALLOCATE_PLT_SOCKETS PLT_ALLOCATE_tls(lib_socket_if_t, SOCKET_LIB_IF_LIST)

#define LIB_SOCKET_DATA OTHER_DOMAIN_DATA(lib_socket_if_t)

// Some helpful macros as using poll has a lot of cookie cutter at present =(
#define POLL_LOOP_START(sleep_var, event_var, messages)             \
    int sleep_var = 0;                                              \
    int event_var = 0;                                              \
                                                                    \
    while(1) {                                                      \
                                                                    \
        if(sleep_var) syscall_cond_cancel();                        \
        event_var = 0;


#define POLL_ITEM_F(event, sleep_var, event_var, item, events, from_check)                          \
    enum poll_events event = socket_fulfill_poll(item, events, sleep_var, from_check, 0);  \
    if(event) {                                                                                     \
        event_var = 1;                                                                              \
        sleep_var = 0;                                                                              \
    }

#define POLL_ITEM_R(event, sleep_var, event_var, item, events, space)                               \
    enum poll_events event = socket_request_poll(item, events, sleep_var, space);          \
    if(event) {                                                                                     \
        event_var = 1;                                                                              \
        sleep_var = 0;                                                                              \
    }


#define POLL_LOOP_END(sleep_var, event_var, messages, timeout)      \
    if(!event_var) {                                                \
        if(sleep_var) {                                             \
            if(messages) msg_entry(timeout - 1, MSG_ENTRY_TIMEOUT_ON_NOTIFY | MSG_ENTRY_TIMEOUT_ON_MESSAGE);\
            else syscall_cond_wait(0, timeout);                     \
        }                                                           \
        sleep_var = 1;                                              \
    }                                                               \
}

#define POLL_LOOP_CONTINUE continue

enum socket_connect_type {
    CONNECT_NONE = 0,
    CONNECT_PULL_READ = 1,
    CONNECT_PULL_WRITE = 2,
    CONNECT_PUSH_READ = 4,
    CONNECT_PUSH_WRITE = 8
};

typedef union {
    requester_t pull_reader;
    fulfiller_t push_reader;
} socket_reader_t;
typedef union {
    requester_t push_writer;
    fulfiller_t pull_writer;
} socket_writer_t;

typedef enum {
    ASYNC_NEED_FLUSH_DRB = 0,
    ASYNC_NEED_REQ_CLOSE,
    ASYNC_NEED_REQS_WRITE,
    ASYNC_NEED_REQS_READ,
    ASYNC_FREE_RES,
    ASYNC_DONE
} asyn_close_state_e;

struct unix_like_socket;

typedef ssize_t close_fun(struct unix_like_socket* sock);
typedef enum poll_events custom_poll_f(struct unix_like_socket* sock, enum poll_events asked_events, int set_waiting);

// Bi directional unix like socket
typedef struct unix_like_socket {
    enum SOCKET_FLAGS flags;
    enum socket_connect_type con_type;
    asyn_close_state_e close_state;
    uint8_t sockn;
    close_fun* custom_close;
    custom_poll_f* custom_poll;
    struct unix_like_socket* delay_close_next;
    data_ring_buffer write_copy_buffer; // If we push write and are worried about delays we need a buffer
    // data_ring_buffer read_copy_buffer; // Do we copy for pull reading? Otherwise how do we know when consume has happened?
    // If we emulate a single pointer these are used to track how far behind we are with read/write
    uint64_t read_behind;
    uint64_t write_behind;
    socket_reader_t read;
    socket_writer_t write;
    locked_t encrypt_lock;
} unix_like_socket;



int init_data_buffer(data_ring_buffer* buffer, char* char_buffer, uint32_t data_buffer_size);

// Unix like interface. Either copies or waits for fulfill to return //

// This only inits the unix_socket struct. Up to you to provide underlying.

ssize_t socket_requester_request_wait_for_fulfill(requester_t requester, char* buf, uint64_t length);

int socket_init(unix_like_socket* sock, enum SOCKET_FLAGS flags,
                char* data_buffer, uint32_t data_buffer_size,
                enum socket_connect_type con_type);
ssize_t socket_close(unix_like_socket* sock);
ssize_t socket_recv(unix_like_socket* sock, char* buf, size_t length, enum SOCKET_FLAGS flags);
ssize_t socket_send(unix_like_socket* sock, const char* buf, size_t length, enum SOCKET_FLAGS flags);

ssize_t socket_sendfile(unix_like_socket* sockout, unix_like_socket* sockin, size_t count);

void catch_up_write(unix_like_socket* file);
void catch_up_read(unix_like_socket* file);
ssize_t socket_flush_drb(unix_like_socket* socket);

typedef struct poll_sock {
    unix_like_socket* fd;
    enum poll_events events;
    enum poll_events revents;
} poll_sock_t;

int socket_poll(poll_sock_t* socks, size_t nsocks, int timeout, enum poll_events* msg_queue_poll);

int assign_socket_n(unix_like_socket* sock);

requester_t socket_malloc_requester_32(uint8_t socket_type, data_ring_buffer *paired_drb);
fulfiller_t socket_malloc_fulfiller(uint8_t socket_type);

ssize_t socket_requester_lseek(requester_t requester, int64_t offset, int whence, int dont_wait);
int socket_listen_rpc(register_t port,
                      requester_t requester,
                      fulfiller_t fulfiller);
int socket_connect_via_rpc(act_kt target, register_t port,
                           requester_t requester,
                           fulfiller_t fulfiller);

#endif //CHERIOS_SOCKETS_H
