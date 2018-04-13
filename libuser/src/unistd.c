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

#include "unistd.h"
#include "stdlib.h"
#include "namespace.h"
#include "stdio.h"

act_kt fs_act;

act_kt try_get_fs(void) {
    if(!fs_act) {
        fs_act = namespace_get_ref(namespace_num_fs);
    }
    return fs_act;
}

FILE_t open(const char* name, int read, int write, enum SOCKET_FLAGS flags) {
    if(!read && !write) return NULL;

    act_kt dest = try_get_fs();

    if(!dest) return NULL;

    struct requester_32* r32_read = NULL, *r32_write = NULL;

    enum socket_connect_type  con_type = CONNECT_NONE;

    if(read) {
        con_type |= CONNECT_PULL_READ;
        r32_read = (struct requester_32*)malloc(SIZE_OF_request(32));
        socket_internal_requester_init(&r32_read->r, 32, SOCK_TYPE_PULL, NULL);
    }
    if(write) {
        con_type |= CONNECT_PUSH_WRITE;
        r32_write = (struct requester_32*)malloc(SIZE_OF_request(32));
        socket_internal_requester_init(&r32_write->r, 32, SOCK_TYPE_PUSH, NULL);
    }

    ssize_t result;

    if((result = message_send(0,0,0,0,r32_read, r32_write, name, NULL, dest, SYNC_CALL, 0))) goto er1;
    if(r32_read) socket_internal_requester_connect(&r32_read->r);
    if(r32_write) socket_internal_requester_connect(&r32_write->r);

    unix_like_socket* sock = (unix_like_socket*)(malloc(sizeof(unix_like_socket)));

    if((result = socket_init(sock, flags | MSG_NO_COPY, NULL, 0, con_type))) goto er2;

    sock->write.push_writer = &r32_write->r;
    sock->read.pull_reader = &r32_read->r;

    return sock;

    er2:
    free(sock);
    er1:
    free(r32_read);
    free(r32_write);
    return NULL;
}

ssize_t close(FILE_t file) {
    ssize_t result = socket_close(file);
    if(result == 0) {
        if(file->con_type | CONNECT_PULL_READ) free(file->read.pull_reader);
        if(file->con_type | CONNECT_PUSH_WRITE) free(file->write.push_writer);
        free(file);
    }
    return result;
}

ssize_t write(FILE_t file, const void* buf, size_t nbyte) {
    return socket_send(file, buf, nbyte, MSG_NONE);
}

ssize_t read(FILE_t file, void* buf, size_t nbyte) {
    return socket_recv(file, buf, nbyte, MSG_NONE);
}

ssize_t lseek(FILE_t file, int64_t offset, int whence) {
    return socket_seek(file, offset, whence);
}