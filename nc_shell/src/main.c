/*-
 * Copyright (c) 2019 Lawrence Esswood
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
#include "net.h"

// Implements a shell over tcp to be connected to by netcat

#define NC_SHELL_PORT 6666

typedef struct {
    NET_SOCK ns;
} nc_shell_state;

extern ssize_t TRUSTED_CROSS_DOMAIN(ns_ful) (capability arg, char* buf, uint64_t offset, uint64_t length);
__used ssize_t ns_ful(capability arg, char* buf, __unused uint64_t offset, uint64_t length) {
    nc_shell_state* shellState = (nc_shell_state*)arg;

    // TODO actually write a shell

    fprintf((FILE*)shellState->ns, "I have not actually written a shell. Heres what you typed: %*.*s\n", (int)length, (int)length, buf);

    return length;
}

void shell_start(__unused register_t arg, capability carg) {

    NET_SOCK ns = (NET_SOCK)carg;
    // We want direct access to the underlying sockets

    fulfiller_t ful = ns->sock.read.push_reader;

    ssize_t res;

    nc_shell_state shellState;

    shellState.ns = ns;

    do {
        res = socket_fulfill_progress_bytes_unauthorised(ful, SOCK_INF, F_PROGRESS | F_CHECK,
                                                         &TRUSTED_CROSS_DOMAIN(ns_ful), &shellState, 0, NULL, NULL, TRUSTED_DATA,
                                                         NULL);
    } while(res > 0);

    printf("net shell disconnected with %ld\n", res);
}


int main(__unused register_t arg, __unused capability carg) {

    // Set up a TCP server

    struct tcp_bind bind;
    bind.addr.addr = IP_ADDR_ANY->addr;
    bind.port = NC_SHELL_PORT;
    listening_token_or_er_t token_or_er = netsock_listen_tcp(&bind, 1, NULL, NULL);

    assert(IS_VALID(token_or_er));

    __unused listening_token tok = token_or_er.val;

    char tn[] = "shell_thread00";

    while(1) {
        // Accept and spawn a new thread
        NET_SOCK ns = netsock_accept(0);

        printf("Someone has connected to the netcat shell!\n");

        thread_new(tn, 0, (capability)ns, &shell_start);

        if(tn[13] == '9') {
            tn[12] ++;
            tn[13] = '0';
        } else tn[13]++;
    }
}
