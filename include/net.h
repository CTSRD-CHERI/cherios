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

#define LWIP_SOCKET_TYPES           1
#define LWIP_SOCKET                 0

#include "sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/sockets.h" // We don't use any of the socket functions. But the types are useful for compat

struct tcp_bind {
    ip_addr_t addr;
    uint16_t port;
};

typedef capability listening_token;

DEC_ERROR_T(listening_token);

struct net_sock {
    unix_like_socket sock;
    struct tcp_bind bind;
    struct net_sock* next_to_accept; // Inline linked list to allow out of order accepts
    capability callback_arg;
    struct requester_32 write_req;
};

// Without inline drb or inline reqs as this might just be a listen socket
typedef struct unix_net_sock {
    unix_like_socket sock;
    struct tcp_bind bind;
    struct net_sock* next_to_accept; // Inline linked list to allow out of order accepts
    capability callback_arg;
    listening_token token;
} unix_net_sock;

// TODO: Reduce again when I teach NGINX about AIO
#define NET_SOCK_DRB_SIZE 0x20000

typedef struct net_sock* NET_SOCK;

act_kt net_try_get_ref(void);

listening_token_or_er_t netsock_listen_tcp(struct tcp_bind* bind, uint8_t backlog,
                       capability callback_arg);
void netsock_stop_listen(listening_token token);

int netsock_connect_tcp(struct tcp_bind* bind, struct tcp_bind* server,
                        capability callback_arg);

// Accepts anything. Find out what using the callback_arg
NET_SOCK netsock_accept(enum SOCKET_FLAGS flags);
// Same again but does not alloc and uses in (if in is null this is the same as netsock_accept)
NET_SOCK netsock_accept_in(enum SOCKET_FLAGS flags, NET_SOCK in);

// Accepts but filters for correct unix socket and puts others on wait lists
NET_SOCK accept_until_correct(unix_net_sock* expect, int dont_wait);

// Puts one incoming connecting on wait list
void accept_one(int dont_wait);

struct hostent {
    const char  *h_name;       /* official name of host */
    char **h_aliases;         /* alias list */
    int    h_addrtype;        /* host address type */
    int    h_length;          /* length of address */
    char **h_addr_list;       /* list of addresses */
};

struct hostent *gethostbyname(const char *name);

/******************************/
/* A more unix like interface */
/******************************/

typedef  unix_net_sock* unix_net_sock_ptr;
DEC_ERROR_T(unix_net_sock_ptr);

ERROR_T(unix_net_sock_ptr) socket_or_er(int domain, int type, int protocol);

unix_net_sock* socket(int domain, int type, int protocol);

int bind(unix_net_sock* sockfd, const struct sockaddr *addr,
         socklen_t addrlen);

int listen(unix_net_sock* sockfd, int backlog);

int connect(unix_net_sock* socket, const struct sockaddr *address,
            socklen_t address_len);

NET_SOCK accept(unix_net_sock* sockfd, struct sockaddr *addr, socklen_t *addrlen);

NET_SOCK accept4(unix_net_sock* sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);

int shutdown(NET_SOCK sockfd, int how);

#endif //CHERIOS_NET_H
