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

#include <queue.h>
#include "cheric.h"
#include "net.h"
#include "assert.h"
#include "sockets.h"
#include "stdlib.h"
#include "namespace.h"

act_kt net_act;
act_kt net_try_get_ref(void) {
    if(!net_act) {
        net_act = namespace_get_ref(namespace_num_tcp);
    }
    return net_act;
}

int netsock_close(NET_SOCK sock) {
    socket_internal_requester_wait_all_finish(sock->sock.write.push_writer, 0);
    socket_close(&sock->sock);
    free((capability)sock);
    return 0;
}


#define TCP_CALLBACK_PORT 123

int netsock_listen_tcp(struct tcp_bind* bind, uint8_t backlog,
                       capability callback_arg) {
    act_kt act = net_try_get_ref();
    assert(act != NULL);
    return (int)message_send(backlog, TCP_CALLBACK_PORT, 0, 0, bind, act_self_ref, callback_arg, NULL, act, SEND, 1);
}

int netsock_connect_tcp(struct tcp_bind* bind, struct tcp_bind* server,
                        capability callback_arg) {
    act_kt act = net_try_get_ref();
    assert(act != NULL);
    return (int)message_send(TCP_CALLBACK_PORT, 0, 0, 0, bind, server, act_self_ref, callback_arg, act, SEND, 0);
}

NET_SOCK netsock_accept(enum SOCKET_FLAGS flags) {
    int dont_wait = flags & MSG_DONT_WAIT;

    if(msg_queue_empty()) {
        if(dont_wait) return NULL;
        wait();
    }
    msg_t* msg = get_message();

    if(msg->v0 != TCP_CALLBACK_PORT) {
        printf("BAD PORT NUMBER IN TCP CALLBACK");
        return NULL;
    }

    capability callback = msg->c3;
    capability session_token = msg->c4;
    uni_dir_socket_requester* requester = (uni_dir_socket_requester*)msg->c5;
    int err = (int)msg->a0;

    assert_int_ex(err, ==, 0);

    next_msg();

    uint8_t drb_inline = (uint8_t)!(flags & MSG_NO_COPY);
    // Alloc netsock

    size_t size = sizeof(struct net_sock);
    if(drb_inline) size+=NET_SOCK_DRB_SIZE;

    NET_SOCK sock = (NET_SOCK)malloc(size);
    sock->callback_arg = callback;
    sock->drb_inline = drb_inline;
    char* drb = NULL;
    if(sock->drb_inline) {
        drb = ((char*)sock) + sizeof(struct net_sock);
    }

    socket_internal_requester_init(&sock->write_req.r, 32, SOCK_TYPE_PUSH, NULL);
    socket_internal_fulfiller_init(&sock->sock.read.push_reader, SOCK_TYPE_PUSH);
    socket_init(&sock->sock, flags, drb, sock->drb_inline ? NET_SOCK_DRB_SIZE : 0, CONNECT_PUSH_READ | CONNECT_PUSH_WRITE);
    sock->sock.write.push_writer = &sock->write_req.r;

    // Send message to net_act

    act_kt net = net_try_get_ref();

    message_send(0,0,0,0, session_token, socket_internal_make_read_only(&sock->write_req.r), NULL, NULL, net, SEND, 2);

    assert(requester != NULL);

    socket_internal_requester_connect(&sock->write_req.r);
    socket_internal_fulfiller_connect(&sock->sock.read.push_reader, requester);

    return sock;
}