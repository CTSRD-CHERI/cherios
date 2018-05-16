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
#ifndef CHERIOS_NET_H
#define CHERIOS_NET_H

#include "sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/inet.h"

struct tcp_bind {
    ip_addr_t addr;
    uint16_t port;
};
struct tcp_con_args {
    struct tcp_bind binding;
    err_t result;
};

struct net_sock {
    unix_like_socket sock;
    capability callback_arg;
    struct requester_32 write_req;
    uint8_t drb_inline;
    data_ring_buffer drb;
};

#define NET_SOCK_DRB_SIZE 0x4000

typedef struct net_sock* NET_SOCK;

enum TCP_OOBS {
    REQUEST_TCP_CONNECT = (2 << 16) | (0x10),
    REQUEST_TCP_LISTEN  = (2 << 16) | (0x11),
};


act_kt net_try_get_ref(void);

int netsock_close(NET_SOCK sock);
int netsock_listen_tcp(struct tcp_bind* bind, uint8_t backlog,
                       capability callback_arg);
int netsock_connect_tcp(struct tcp_bind* bind, struct tcp_bind* server,
                        capability callback_arg);
NET_SOCK netsock_accept(enum SOCKET_FLAGS flags);
#endif //CHERIOS_NET_H
