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
#include <sockets.h>
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

#define TCP_CALLBACK_PORT 123

listening_token_or_er_t netsock_listen_tcp(struct tcp_bind* bind, uint8_t backlog,
                       capability callback_arg) {
    act_kt act = net_try_get_ref();
    assert(act != NULL);
    return (listening_token_or_er_t)message_send_c(backlog, TCP_CALLBACK_PORT, 0, 0, bind, act_self_ref, callback_arg, NULL, act, SEND, 1);
}

void netsock_stop_listen(listening_token token) {
    message_send(0,0,0,0,token,NULL,NULL,NULL, net_act, SEND, 4);
    return;
}

int netsock_connect_tcp(struct tcp_bind* bind, struct tcp_bind* server,
                        capability callback_arg) {
    act_kt act = net_try_get_ref();
    assert(act != NULL);
    return (int)message_send(TCP_CALLBACK_PORT, 0, 0, 0, bind, server, act_self_ref, callback_arg, act, SEND, 0);
}

NET_SOCK netsock_accept_in(enum SOCKET_FLAGS flags, NET_SOCK in) {

    int dont_wait = flags & MSG_DONT_WAIT;

    if(msg_queue_empty()) {
        if(dont_wait) return NULL;
        wait();
    }
    msg_t* msg = get_message();

    if(msg->v0 != TCP_CALLBACK_PORT) {
        printf("BAD PORT NUMBER IN TCP CALLBACK");
        assert(0);
        return NULL;
    }

    capability callback = msg->c3;
    capability session_token = msg->c4;
    uni_dir_socket_requester* requester = (uni_dir_socket_requester*)msg->c5;
    int err = (int)msg->a0;

    next_msg();

    if(err != 0) return NULL;

    uint8_t drb_inline = (uint8_t)!(flags & MSG_NO_COPY);
    // Alloc netsock

    NET_SOCK sock = in;

    if(!in) {
        flags |= SOCKF_WR_INLINE;
        size_t size = sizeof(struct net_sock);

        char* drb = NULL;
        uint32_t drb_size = 0;

        if(drb_inline) {
            size+=NET_SOCK_DRB_SIZE;
            flags |= SOCKF_DRB_INLINE;
            drb_size = NET_SOCK_DRB_SIZE;
        }

        sock = (NET_SOCK)malloc(size);

        if(drb_inline) drb = ((char*)sock) + sizeof(struct net_sock);
        uni_dir_socket_requester* write_req = &sock->write_req.r;

        socket_internal_fulfiller_init(&sock->sock.read.push_reader, SOCK_TYPE_PUSH);
        socket_internal_requester_init(&sock->write_req.r, 32, SOCK_TYPE_PUSH, drb ? &sock->sock.write_copy_buffer : NULL);
        socket_init(&sock->sock, flags, drb, drb_size, CONNECT_PUSH_READ | CONNECT_PUSH_WRITE);
        sock->sock.write.push_writer = write_req;
    }



    sock->callback_arg = callback;

    // Send message to net_act

    act_kt net = net_try_get_ref();

    message_send(0,0,0,0, session_token, socket_internal_make_read_only(sock->sock.write.push_writer), NULL, NULL, net, SEND, 2);

    assert(requester != NULL);

    socket_internal_requester_connect(&sock->write_req.r);
    socket_internal_fulfiller_connect(&sock->sock.read.push_reader, requester);

    return sock;
}

NET_SOCK netsock_accept(enum SOCKET_FLAGS flags) {
    return netsock_accept_in(flags, NULL);
}

struct hostent_names {
    struct hostent he;
    char* aliases[1];        // Always set to null, no aliases
    char* addr_list[2];      // A single address
    u32_t addr;
};

struct hostent *gethostbyname(const char *name) {
    act_kt net = net_try_get_ref();

    if(net == NULL) return  NULL;

    // Gets a single ip_addr_t by value and then constructs the stupid unix structure....
    u32_t addr = message_send(0,0,0,0,name,NULL,NULL,NULL,net,SYNC_CALL,3);

    if(addr == (u32_t)-1) return NULL;

    struct hostent_names* he = (struct hostent_names*)malloc(sizeof(struct hostent_names));

    he->he.h_name = name;

    he->he.h_aliases = (char**)&he->aliases;
    he->aliases[0] = NULL;

    he->he.h_addr_list = (char**)&he->addr_list;
    he->he.h_addrtype = AF_INET;
    he->he.h_length = 4; // Do we want the address as an int or string?
    he->addr_list[0] = (char*)&he->addr;
    he->addr = addr;
    he->addr_list[1] = NULL;

    return &he->he;
}

static void sockaddr_to_bind(const struct sockaddr *addr, struct tcp_bind* bind) {
    struct sockaddr_in* sock_in = (struct sockaddr_in*)addr;
    bind->port = sock_in->sin_port;
    bind->addr.addr = sock_in->sin_addr.s_addr;
}

// Creates a socket large enough for either a listening state thing, or a unix like socket but with no inline stuf
unix_net_sock* socket(int domain, int type, int protocol) {
    assert_int_ex(protocol, ==, 0);
    assert_int_ex(domain, ==, AF_INET);
    assert_int_ex(type, ==, SOCK_STREAM);

    // Allocate
    unix_net_sock* uns = (unix_net_sock*)malloc(sizeof(unix_net_sock));

    // Init fulfill and socket. Others come in connect
    ssize_t ret = socket_internal_fulfiller_init(&uns->sock.read.push_reader, SOCK_TYPE_PUSH);
    assert(ret == 0);
    ret = socket_init(&uns->sock, MSG_NONE, NULL, 0, CONNECT_PUSH_READ | CONNECT_PUSH_WRITE);
    assert(ret == 0);

    return (unix_like_socket*)uns;
}

int bind(unix_net_sock* sockfd, const struct sockaddr *addr,
         socklen_t addrlen) {
    sockaddr_to_bind(addr, &sockfd->bind);
    return 0;
}

static int close_listen(unix_net_sock* sock) {
    netsock_stop_listen(sock->token);
    free(sock);
}

int listen(unix_net_sock* sockfd, int backlog) {
    listening_token_or_er_t res = netsock_listen_tcp(&sockfd->bind, backlog, sockfd);
    if(!IS_VALID(res)) return res.er;

    sockfd->sock.custom_close = &close_listen;
    sockfd->token =  res.val;
    return 0;
}

static NET_SOCK accept_until_correct(unix_net_sock* expect) {
    NET_SOCK out_order = expect->next_to_accept;
    if(out_order) {
        expect->next_to_accept = out_order->next_to_accept;
        return out_order;

    }

    while(1) {
        NET_SOCK ns = netsock_accept(MSG_DONT_WAIT);

        assert(ns != NULL);

        if(ns->callback_arg == expect) return ns;

        unix_net_sock* listener = ns->callback_arg;

        ns->next_to_accept = listener->next_to_accept;

        listener->next_to_accept = ns;
    }


}

static void accept_one(void) {
    NET_SOCK ns = netsock_accept(MSG_DONT_WAIT);

    assert(ns != NULL);

    unix_net_sock* listener = ns->callback_arg;
    ns->next_to_accept = listener->next_to_accept;
    listener->next_to_accept = ns;
}

typedef struct {
    struct requester_32 write_req;
    char drb_buf[NET_SOCK_DRB_SIZE];
} netsock_req;

int connect(unix_net_sock* socket, const struct sockaddr *address,
            socklen_t address_len) {

    int found = 0;

    do {
        if(msg_queue_empty()) {
            wait();
        }
        msg_t* msg = get_message();

        if(msg->v0 != TCP_CALLBACK_PORT) {
            printf("BAD PORT NUMBER IN TCP CALLBACK");
            assert(0);
            return NULL;
        }

        capability callback = msg->c3;

        if(callback != socket) {
            accept_one();
        } else found = 1;

    } while(!found);

    // alloc a drb buffer and write requester
    int flags = SOCKF_DRB_INLINE; // DRB is inline with requests

    netsock_req* nsr = (netsock_req*)malloc(sizeof(netsock_req));

    // init the requester and drb

    uni_dir_socket_requester* requester = &nsr->write_req;
    char* buf = nsr->drb_buf;

    init_data_buffer(&socket->sock.write_copy_buffer, buf, NET_SOCK_DRB_SIZE);
    socket_internal_requester_init(requester,32,SOCK_TYPE_PUSH,&socket->sock.write_copy_buffer);
    socket->sock.write.push_writer = nsr;

    // now make the netsock
    NET_SOCK ns = netsock_accept_in(flags, (NET_SOCK)socket);

    assert(ns != NULL);

    return 0;
}

NET_SOCK accept(unix_net_sock* sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return accept4(sockfd, addr, addrlen, MSG_NONE);
}

NET_SOCK accept4(unix_net_sock* sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {

    NET_SOCK ns = accept_until_correct(sockfd);

    flags |= sockfd->sock.flags;

    if((flags ^ ns->sock.flags) & SOCKF_GIVE_SOCK_N) {
        // TODO
        ns->sock.flags |= SOCKF_GIVE_SOCK_N;
    }

    return ns;
}
