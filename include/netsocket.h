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
#ifndef CHERIOS_NETSOCKET_H
#define CHERIOS_NETSOCKET_H

#include "sockets.h"

typedef struct netsocket {
    unix_like_socket sock;
    struct requester_32 write_req;
    char drb[0x200];
} netsocket;

static inline int netsocket_init(netsocket* netsock) {
    socket_init(&netsock->sock, MSG_NONE, netsock->drb, sizeof(netsock->drb), CONNECT_PUSH_WRITE | CONNECT_PUSH_READ);
    netsock->sock.write.push_writer = &netsock->write_req.r;
    socket_internal_requester_init(&netsock->write_req.r, 32, SOCK_TYPE_PUSH, &netsock->sock.write_copy_buffer);
    socket_internal_fulfiller_init(&netsock->sock.read.push_reader, SOCK_TYPE_PUSH);
    return 0;
}

static inline int netsocket_listen(netsocket* netsock, register_t port) {
    // TODO registeron some lower layer that we are listening on port
    return socket_internal_listen(port, &netsock->write_req.r, &netsock->sock.read.push_reader);
}

// TODO connect to IP. Also the ref should be an ip handler, which should then perform a loopback
static inline int netsocket_connect(netsocket* netsock, register_t port, act_kt ref) {
    return socket_internal_connect(ref, port, &netsock->write_req.r, &netsock->sock.read.push_reader);
}

#endif //CHERIOS_NETSOCKET_H
