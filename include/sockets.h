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
#ifndef CHERIOS_SOCKETS_H
#define CHERIOS_SOCKETS_H

#include "object.h"

#define SOCKET_CONNECT_IPC_NO       (0xbeef)

#define E_AGAIN                     (-1)
#define E_MSG_SIZE                  (-2)
#define E_COPY_NEEDED               (-11)
#define E_UNSUPPORTED               (-12)

#define E_CONNECT_FAIL              (-3)
#define E_CONNECT_FAIL_WRONG_PORT   (-9)
#define E_CONNECT_FAIL_WRONG_TYPE   (-10)

#define E_BUFFER_SIZE_NOT_POWER_2   (-4)
#define E_ALREADY_CLOSED            (-5)
#define E_SOCKET_CLOSED             (-6)
#define E_SOCKET_WRONG_TYPE         (-7)
#define E_SOCKET_NO_DIRECTION       (-8)

enum SOCKET_FLAGS {
    MSG_NONE = 0,
    MSG_DONT_WAIT = 1,
    MSG_NO_CAPS = 2,
    MSG_NO_COPY = 4,
};

#define SOCK_TYPE_PUSH 0
#define SOCK_TYPE_PULL 1

// Uni-directional socket.
typedef struct uni_dir_socket_requester_fulfiller_component  {
    volatile act_kt fulfiller_waiting;
    volatile act_kt requester_waiting;
    volatile uint16_t fulfill_ptr;
    volatile uint8_t  fulfiller_closed;
} uni_dir_socket_requester_fulfiller_component;

typedef struct uni_dir_socket_requester {
    uni_dir_socket_requester_fulfiller_component fulfiller_component;
    volatile uint8_t requester_closed;
    uint8_t socket_type;
    uint16_t buffer_size;
    volatile uint16_t requeste_ptr;
    uni_dir_socket_requester_fulfiller_component* access;
    char* volatile   indirect_ring_buffer[];
} uni_dir_socket_requester;

#define SIZE_OF_request(buffer_size) ((sizeof(char*) * buffer_size) + sizeof(uni_dir_socket_requester))

typedef struct uni_dir_socket_fulfiller {
    uni_dir_socket_requester* requester;  // Read only
    size_t partial_fulfill_bytes;
    uint8_t socket_type;
} uni_dir_socket_fulfiller;

// Ring buffer for copy in for unix abstraction
typedef struct data_ring_buffer {
    size_t requeste_ptr;
    size_t fulfill_ptr;
    size_t buffer_size;
    uint16_t last_processed_fulfill_ptr; // From the indirect buffer. Can fall out of sync.
    char* buffer;
} data_ring_buffer;

enum socket_connect_type {
    CONNECT_NONE = 0,
    CONNECT_PULL_READ = 1,
    CONNECT_PULL_WRITE = 2,
    CONNECT_PUSH_READ = 4,
    CONNECT_PUSH_WRITE = 8
};

typedef union {
    uni_dir_socket_requester* pull_reader;
    uni_dir_socket_fulfiller push_reader;
} socket_reader_t;
typedef union {
    uni_dir_socket_requester* push_writer;
    uni_dir_socket_fulfiller pull_writer;
} socket_writer_t;

// Bi directional unix like socket
typedef struct unix_like_socket {
    enum SOCKET_FLAGS flags;
    enum socket_connect_type con_type;
    data_ring_buffer write_copy_buffer; // If we push write and are worried about delays we need a buffer
    // data_ring_buffer read_copy_buffer; // Do we copy for pull reading? Otherwise how do we know when consume has happened?
    socket_reader_t read;
    socket_writer_t write;
} unix_like_socket;


typedef uni_dir_socket_requester* requester_ptr_t;
DEC_ERROR_T(requester_ptr_t);


// Init
int socket_internal_fulfiller_init(uni_dir_socket_fulfiller* fulfiller, uint8_t socket_type);
int socket_internal_requester_init(uni_dir_socket_requester* requester, uint16_t buffer_size, uint8_t socket_type);


// Connection
int socket_internal_listen(register_t port,
                           uni_dir_socket_requester* requester,
                           uni_dir_socket_fulfiller* fulfiller);
int socket_internal_connect(act_kt target, register_t port,
                            uni_dir_socket_requester* requester,
                            uni_dir_socket_fulfiller* fulfiller);

// Closing
int socket_internal_close_requester(uni_dir_socket_requester* requester);
int socket_internal_close_fulfiller(uni_dir_socket_fulfiller* fulfiller);

// Request/Fulfill

// Call these first to ensure space/requests outstanding
int socket_internal_requester_space_wait(uni_dir_socket_requester* requester, uint16_t need_space, int dont_wait);
int socket_internal_fulfill_peek(uni_dir_socket_fulfiller* fulfiller, uint16_t amount, int dont_wait);

// Then do stuff with the buffer
ssize_t socket_internal_fulfill(uni_dir_socket_fulfiller* fulfiller, char* buf, size_t length, enum SOCKET_FLAGS flags);

// Call these after to request/progress
int socket_internal_requester_request(uni_dir_socket_requester* requester, char* buffer, int dont_wait);
int socket_internal_fulfill_progress(uni_dir_socket_fulfiller* fulfiller, uint16_t amount, int dont_wait);




// Unix like interface. Either copies or waits for fulfill to return.

// This only inits the unix_socket struct. Up to you to provide underlying.
int socket_init(unix_like_socket* sock, enum SOCKET_FLAGS flags,
                char* data_buffer, size_t data_buffer_size,
                enum socket_connect_type con_type);

ssize_t socket_recv(unix_like_socket* sock, char* buf, size_t length, enum SOCKET_FLAGS flags);
ssize_t socket_send(unix_like_socket* sock, const char* buf, size_t length, enum SOCKET_FLAGS flags);


#endif //CHERIOS_SOCKETS_H
