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
#include "sockets.h"
#include "cheric.h"
#include "net.h"
#include "assert.h"
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

sealing_cap get_ethernet_sealing_cap(void) {
    act_kt act = net_try_get_ref();
    // TODO we should be getting a certified sealing cap
    sealing_cap sc = message_send_c(0,0, 0, 0, NULL, NULL, NULL, NULL, act, SYNC_CALL, 5);
    assert(sc != NULL);
    return sc;
}

listening_token_or_er_t netsock_listen_tcp(struct tcp_bind* bind, uint8_t backlog,
                       capability callback_arg) {
    act_kt act = net_try_get_ref();
    assert(act != NULL);
    capability res = message_send_c(backlog, TCP_CALLBACK_PORT, 0, 0, bind, act_self_ref, callback_arg, NULL, act, SYNC_CALL, 1);
    return MAKE_VALID(listening_token, res);
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

    uint16_t port = msg->a1;
    uint32_t addr = msg->a2;

    requester_t requester = (requester_t*)msg->c5;
    int err = (int)msg->a0;

    next_msg();

    if(err != 0) return NULL;

    uint8_t drb_inline = (uint8_t)!(flags & MSG_NO_COPY);
    // Alloc netsock

    NET_SOCK sock = in;

    if(!in) {
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
        requester_t write_req = socket_malloc_requester_32(SOCK_TYPE_PUSH, drb ? &sock->sock.write_copy_buffer : NULL);
        fulfiller_t ful = socket_malloc_fulfiller(SOCK_TYPE_PUSH);

        socket_init(&sock->sock, flags, drb, drb_size, CONNECT_PUSH_READ | CONNECT_PUSH_WRITE);
        sock->sock.write.push_writer = write_req;
        sock->sock.read.push_reader = ful;
    }


    sock->bind.addr.addr = addr;
    sock->bind.port = port;
    sock->callback_arg = callback;

    // Send message to net_act

    act_kt net = net_try_get_ref();

    message_send(0,0,0,0, session_token, socket_make_ref_for_fulfill(sock->sock.write.push_writer), NULL, NULL, net, SEND, 2);

    assert(requester != NULL);

    socket_requester_connect(sock->sock.write.push_writer);
    sealing_cap sc = get_ethernet_sealing_cap();
    socket_requester_restrict_seal(sock->sock.write.push_writer, sc);
    socket_fulfiller_connect(sock->sock.read.push_reader, requester);

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
    const struct sockaddr_in* sock_in = (const struct sockaddr_in*)addr;
    bind->port = sock_in->sin_port;
    bind->addr.addr = sock_in->sin_addr.s_addr;
}

static void bind_to_sockaddr(struct sockaddr *addr, const struct tcp_bind* bind) {
    addr->sa_family = AF_INET;
    struct sockaddr_in* sock_in = (struct sockaddr_in*)addr;
    sock_in->sin_port = bind->port;
    sock_in->sin_addr.s_addr = bind->addr.addr;
}

// Creates a socket large enough for either a listening state thing, or a unix like socket but with no inline stuff
ERROR_T(unix_net_sock_ptr) socket_or_er(__unused int domain, __unused int type, __unused int protocol) {
    assert_int_ex(protocol, ==, 0);
    assert_int_ex(domain, ==, AF_INET);
    assert_int_ex(type, ==, SOCK_STREAM);

    // Allocate
    unix_net_sock* uns = (unix_net_sock*)malloc(sizeof(unix_net_sock));

    fulfiller_t fulfiller = socket_malloc_fulfiller(SOCK_TYPE_PUSH);

    uns->sock.read.push_reader = fulfiller;

    ssize_t  ret = socket_init(&uns->sock, MSG_NONE, NULL, 0, CONNECT_PUSH_READ);

    if(ret < 0) return MAKE_ER(unix_net_sock_ptr,ret);

    return MAKE_VALID(unix_net_sock_ptr,uns);
}

unix_net_sock* socket(int domain, int type, int protocol) {
    ERROR_T(unix_net_sock_ptr) res = socket_or_er(domain, type, protocol);
    return (IS_VALID(res)) ? res.val : NULL;
}

int bind(unix_net_sock* sockfd, const struct sockaddr *addr,
         __unused socklen_t addrlen) {
    sockaddr_to_bind(addr, &sockfd->bind);
    return 0;
}

static ssize_t close_listen(unix_net_sock* sock) {
    netsock_stop_listen(sock->token);
    free(sock);
    return 0;
}

void empty_accept_queue(void) {
    while(1) {
        NET_SOCK ns = netsock_accept(MSG_DONT_WAIT);

        if(ns != NULL) {
            unix_net_sock* listener = (unix_net_sock*)ns->callback_arg;

            ns->next_to_accept = listener->next_to_accept;

            listener->next_to_accept = ns;
        } else break;
    }
}

static enum poll_events poll_listen(unix_like_socket* sock, enum poll_events asked_events, __unused int set_waiting) {
    unix_net_sock* sockfd = (unix_net_sock*)sock;

    if(asked_events & POLL_IN) {
        if(sockfd->next_to_accept == NULL) {
            empty_accept_queue();
        }
        if(sockfd->next_to_accept) return POLL_IN;
    }
    return POLL_NONE;
}

int listen(unix_net_sock* sockfd, int backlog) {
    if(sockfd->token != NULL) return -1; // We don't support changing the backlog via calling listen twice
    listening_token_or_er_t res = netsock_listen_tcp(&sockfd->bind, backlog, sockfd);
    assert(res.val != NULL);
    if(!IS_VALID(res)) return res.er;
    sockfd->sock.custom_close = (close_fun*)&close_listen;
    sockfd->sock.custom_poll = &poll_listen;
    sockfd->token =  res.val;
    return 0;
}

NET_SOCK accept_until_correct(unix_net_sock* expect, int dont_wait) {
    NET_SOCK out_order = expect->next_to_accept;
    if(out_order) {
        expect->next_to_accept = out_order->next_to_accept;
        return out_order;

    }

    while(1) {
        NET_SOCK ns = netsock_accept(dont_wait ? MSG_DONT_WAIT : MSG_NONE);

        if(dont_wait && ns == NULL) return NULL;

        assert(ns != NULL);

        if(ns->callback_arg == expect) return ns;

        unix_net_sock* listener = (unix_net_sock*)ns->callback_arg;

        ns->next_to_accept = listener->next_to_accept;

        listener->next_to_accept = ns;
    }


}

void accept_one(int dont_wait) {
    NET_SOCK ns = netsock_accept(dont_wait ? MSG_DONT_WAIT : MSG_NONE);

    assert(ns != NULL);

    unix_net_sock* listener = (unix_net_sock*)ns->callback_arg;
    ns->next_to_accept = listener->next_to_accept;
    listener->next_to_accept = ns;
}

typedef struct {
    struct requester_32 write_req;
    char drb_buf[NET_SOCK_DRB_SIZE];
} netsock_req;

int connect(unix_net_sock* socket, __unused const struct sockaddr *address,
            __unused socklen_t address_len) {

    int found = 0;

    struct tcp_bind server;

    sockaddr_to_bind(address, &server);

    __unused int res = netsock_connect_tcp(&socket->bind, &server, socket);

    assert(res == 0);

    do {
        if(msg_queue_empty()) {
            wait();
        }
        msg_t* msg = get_message();

        if(msg->v0 != TCP_CALLBACK_PORT) {
            printf("BAD PORT NUMBER IN TCP CALLBACK");
            assert(0);
            return -1;
        }

        capability callback = msg->c3;

        if(callback != socket) {
            accept_one(MSG_NONE);
        } else found = 1;

    } while(!found);

    // alloc a drb buffer and write requester
    int flags = SOCKF_DRB_INLINE; // DRB is inline with requests

    netsock_req* nsr = (netsock_req*)malloc(sizeof(netsock_req));

    // init the requester and drb

    requester_t requester = socket_malloc_requester_32(SOCK_TYPE_PUSH, &socket->sock.write_copy_buffer);
    char* buf = nsr->drb_buf;

    init_data_buffer(&socket->sock.write_copy_buffer, buf, NET_SOCK_DRB_SIZE);

    socket->sock.write.push_writer = requester;
    socket->sock.con_type |= CONNECT_PUSH_WRITE;
    // now make the netsock
    __unused NET_SOCK ns = netsock_accept_in(flags, (NET_SOCK)socket);

    assert(ns != NULL);

    return 0;
}

NET_SOCK accept(unix_net_sock* sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return accept4(sockfd, addr, addrlen, MSG_NONE);
}

NET_SOCK accept4(unix_net_sock* sockfd, struct sockaddr *addr, __unused socklen_t *addrlen, int flags) {

    NET_SOCK ns = accept_until_correct(sockfd, (sockfd->sock.flags | flags) & MSG_DONT_WAIT);

    if(!ns) return NULL;

    flags |= sockfd->sock.flags;

    if((flags ^ ns->sock.flags) & SOCKF_GIVE_SOCK_N) {
        assign_socket_n(&ns->sock);
    }

    if(addr) bind_to_sockaddr(addr, &ns->bind);

    return ns;
}

ssize_t shutdown(NET_SOCK sockfd, int how) {

    if( how & SHUT_RD) {
        assert(0);
    }

    if( how & SHUT_WR) {
        asyn_close_state_e new_state = sockfd->sock.close_state;

        ssize_t res = 0;

        int dont_wait = sockfd->sock.flags & MSG_DONT_WAIT;

        switch (new_state) {
            case ASYNC_NEED_FLUSH_DRB:
                res = socket_flush_drb(&(sockfd->sock));
                if (res == E_SOCKET_CLOSED) res = 0;
                if (res < 0) break;

                new_state = ASYNC_NEED_REQ_CLOSE;
            case ASYNC_NEED_REQ_CLOSE:
                if (sockfd->sock.con_type & CONNECT_PUSH_WRITE) {
                    res = socket_requester_space_wait(sockfd->sock.write.push_writer, 1, dont_wait, 0);
                    if (res == E_SOCKET_CLOSED) res = 0;
                    if (res < 0) break;
                    socket_request_oob(sockfd->sock.write.push_writer, REQUEST_CLOSE, (intptr_t)NULL, 0, 0);
                }

                new_state = ASYNC_NEED_REQS_WRITE;
            default:{}
        }

        sockfd->sock.close_state = new_state;
        return res;
    }

    return -1;
}
