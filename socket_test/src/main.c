/*-
 * Copyright (c) 2017 Lawrence Esswood
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
#include "sockets.h"
#include "thread.h"
#include "assert.h"
#include "stdio.h"
#include "string.h"

#define DATA_SIZE 0x800
#define INDIR_SIZE 16
#define PORT 777

#define BIG_TEST_SIZE 0x2000

struct unix_like_socket_stack {
    unix_like_socket socket;
    char* pad[INDIR_SIZE];
};

const char* str1 = "Hello World!\n";
const int size1 = 14;
const char* str2 = "Some";
const int size2 = 4;
const char* str3 = " more bytes in parts\n";
const int size3 = 22;

const char* str4 = "Some more bytes in parts\n";

void connector_start(register_t arg, capability carg) {
    char data_buffer[DATA_SIZE];
    struct unix_like_socket_stack on_stack;
    unix_like_socket* sock = &on_stack.socket;

    int res = socket_init(sock, MSG_NONE, data_buffer, DATA_SIZE, INDIR_SIZE);
    assert(res == 0);

    int result = socket_internal_connect(carg, PORT, &sock->socket);
    assert(result == 0);

    char buf[100];

    ssize_t rec = socket_recv(sock, buf, size1, MSG_NONE);
    assert(rec == size1);

    // Test a basic send
    assert(strcmp(buf, str1) == 0);

    ssize_t sent = socket_send(sock, str2, size2, MSG_NONE);

    assert(sent == size2);
    sent = socket_send(sock, str3, size3, MSG_NONE);

    assert(sent == size3);

    for(size_t i = 0; i != BIG_TEST_SIZE; i++) {
        rec = socket_recv(sock, buf, 3, MSG_NONE);
        assert(rec == 3);
        assert(buf[0] == (char)(i & 0xFF));
        assert(buf[1] == (char)((i>>8) & 0xFF));
        assert(buf[2] == (char)((i>>16) & 0xFF));
    }

    // Test the closing mechanic

    res = socket_internal_close(&sock->socket);

    assert(res == 0);
    assert(sock->socket.reader.writer->access->reader_closed);
    assert(sock->socket.writer.writer_closed);

    rec = socket_recv(sock, buf, 1, MSG_NONE);

    assert(rec == E_SOCKET_CLOSED);

    sent = socket_send(sock, buf, 1, MSG_NONE);

    assert(sent == E_SOCKET_CLOSED);

    printf("Socket test part2 finished\n");
}

int main(register_t arg, capability carg) {

    printf("Socket test Hello World!\n");

    thread t = thread_new("socket_part2", 0, act_self_ref, &connector_start);

    char data_buffer[DATA_SIZE];
    struct unix_like_socket_stack on_stack;
    unix_like_socket* sock = &on_stack.socket;

    int res = socket_init(sock, MSG_NONE, data_buffer, DATA_SIZE, INDIR_SIZE);
    assert(res == 0);

    int result = socket_internal_listen(PORT, &sock->socket);
    assert(result == 0);

    ssize_t sent = socket_send(sock, str1, size1, MSG_NONE);
    assert(sent == size1);

    char buf[100];

    size_t p1 = 13;
    size_t p2 = size2 + size3 - p1;

    ssize_t rec = socket_recv(sock, buf, p1, MSG_NONE);
    assert(rec == p1);
    rec = socket_recv(sock, buf + p1, p2, MSG_NONE);
    assert(rec == p2);

    // Test multiple sends with partial reads
    assert(strcmp(buf, str4) == 0);

    // Test sending a large number bytes

    for(size_t i = 0; i != BIG_TEST_SIZE; i++) {
        buf[0] = (char)(i & 0xFF);
        buf[1] = (char)((i >> 8) & 0xFF);
        buf[2] = (char)((i >> 16) & 0xFF);
        sent = socket_send(sock, buf, 3, MSG_NONE);
        assert(sent == 3);
    }

    // Test the closing mechanic

    rec = socket_recv(sock, buf, 1, MSG_NONE);

    assert(rec == E_SOCKET_CLOSED);

    assert(!sock->socket.reader.writer->access->reader_closed);
    assert(sock->socket.reader.writer->writer_closed);

    sent = socket_send(sock, buf, 1, MSG_NONE);

    assert(sent == E_SOCKET_CLOSED);

    assert(sock->socket.writer.reader_component.reader_closed);
    assert(!sock->socket.writer.writer_closed);

    res = socket_internal_close(&sock->socket);

    assert(res == 0);
    assert(sock->socket.reader.writer->access->reader_closed);
    assert(sock->socket.writer.writer_closed);

    printf("Socket test finished\n");
}
