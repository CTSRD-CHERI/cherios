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

#define E_AGAIN                     (-1)
#define E_MSG_SIZE                  (-2)
#define E_CONNECT_FAIL              (-3)
#define E_BUFFER_SIZE_NOT_POWER_2   (-4)
#define E_ALREADY_CLOSED            (-5)
#define E_SOCKET_CLOSED             (-6)

enum SOCKET_FLAGS {
    MSG_NONE = 0,
    MSG_DONT_WAIT = 1,
};

// Uni-directional socket.
typedef struct uni_dir_socket_writer_reader_component  {
    volatile act_kt reader_waiting;
    volatile act_kt writer_waiting;
    volatile uint16_t read_ptr;
    volatile uint8_t  reader_closed;
} uni_dir_socket_writer_reader_component;

typedef struct uni_dir_socket_writer {
    uni_dir_socket_writer_reader_component reader_component;
    volatile uint8_t writer_closed;
    uint16_t buffer_size;
    volatile uint16_t write_ptr;
    uni_dir_socket_writer_reader_component* access;
    char* volatile   indirect_ring_buffer[];
} uni_dir_socket_writer;

typedef struct uni_dir_socket_reader {
    uni_dir_socket_writer* writer;  // Read only
    size_t partial_read_bytes;
} uni_dir_socket_reader;

// Ring buffer for copy in for unix abstraction
typedef struct data_ring_buffer {
    size_t write_ptr;
    size_t read_ptr;
    size_t buffer_size;
    uint16_t last_processed_read_ptr; // From the indirect buffer. Can fall out of sync.
    char* buffer;
} data_ring_buffer;

// Bi directional internal socket
typedef struct bi_dir_socket {
    uni_dir_socket_reader reader;
    uni_dir_socket_writer writer;
} bi_dir_socket;

#define SIZE_OF_bi_dir_socket(buffer_size) ((sizeof(char*) * buffer_size) + sizeof(bi_dir_socket))


// Bi directional unix like socket
typedef struct unix_like_socket {
    enum SOCKET_FLAGS flags;
    data_ring_buffer copy_buffer;
    bi_dir_socket socket;
} unix_like_socket;

#define SIZE_OF_unix_like_socket(buffer_size) ((sizeof(char*) * buffer_size) + sizeof(unix_like_socket))

typedef uni_dir_socket_writer* write_ptr_t;
DEC_ERROR_T(write_ptr_t);


int socket_internal_listen(register_t port, bi_dir_socket* sock);
int socket_internal_connect(act_kt target, register_t port, bi_dir_socket* sock);
ssize_t socket_recv(unix_like_socket* sock, char* buf, size_t length, enum SOCKET_FLAGS flags);
ssize_t socket_send(unix_like_socket* sock, const char* buf, size_t length, enum SOCKET_FLAGS flags);
int socket_init(unix_like_socket* sock, enum SOCKET_FLAGS flags, char* data_buffer, size_t data_buffer_size, uint16_t internal_buffer_size);

int socket_internal_close(bi_dir_socket* sock);

#endif //CHERIOS_SOCKETS_H
