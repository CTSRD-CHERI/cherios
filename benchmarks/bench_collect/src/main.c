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
#include "net.h"
#include "namespace.h"
#include "bench_collect.h"

typedef struct {
    unix_net_sock* sock;
    int doing_bench;
    int in_file;
    sync_state_t reps[MAX_Q];
} con_state;

con_state state;



// Protocol:
//  StartFile: 'S'CNH. C = 64 columns. N = null terminated name. H = null terminated headers
//  Send Values: 'D'CV. C = 64 number values, V = 64 values
//  EndFile: 'E'

static inline void connect_to_host(void) {

    // Connect to the host machine
    printf("Benchmark collector connecting...\n");

    state.sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    ip4addr_aton(HOST_IP, (ip4_addr_t*)&server_addr.sin_addr.s_addr);
    ip4addr_aton(CHERIOS_IP, (ip4_addr_t*)&client_addr.sin_addr.s_addr);

    server_addr.sin_port = HOST_PORT;
    client_addr.sin_port = HOST_PORT;

    bind(state.sock, (struct sockaddr*)&client_addr, sizeof(server_addr));

    __unused int res = connect(state.sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

    assert(res == 0);

    printf("Benchmark collector connected\n");
}


static inline int b_start(void) {

    if(state.doing_bench > 0) {
        assert(state.doing_bench -1 != MAX_Q);
        msg_delay_return(&state.reps[state.doing_bench-1]);
    }

    state.doing_bench++;

    return 0;
}

static inline void b_finish_file(void) {
    if(!state.in_file) return;
    fputc('E', &state.sock->sock);
    state.in_file = 0;
}

static inline void b_add_file(size_t columns, char* name, char** headers) {

    requester_t req = state.sock->sock.write.push_writer;

#define W(X) socket_requester_space_wait(req, X, 0, 0)

    b_finish_file();

    char c = 'S';

    W(3);
    socket_request_im(req, 1, NULL, &c, 0);
    socket_request_ind(req, (char*)&columns, 8, 0);
    socket_request_ind(req, name, strlen(name)+1, 0);

    for(size_t i = 0; i != columns; i++) {
        W(1);
        socket_request_ind(req, headers[i], strlen(headers[i])+1, 0);
    }

    socket_requester_wait_all_finish(req, 0);

    state.in_file = 1;
}

static inline void b_add_csv(uint64_t* values, size_t nvalues) {
    requester_t req = state.sock->sock.write.push_writer;

    W(3);
    char c = 'D';
    socket_request_im(req, 1, NULL, &c, 0);
    socket_request_ind(req, (char*)&nvalues, 8, 0);
    socket_request_ind(req, (char*)values, nvalues * sizeof(uint64_t), 0);

    socket_requester_wait_all_finish(req, 0);
}



static inline void b_finish(void) {

    b_finish_file();

    socket_flush_drb(&state.sock->sock);

    state.doing_bench--;

    // Dequeue
    if(state.doing_bench != 0) {
        msg_resume_return(NULL, 0, 0, state.reps[state.doing_bench-1]);
    }
}

int main(__unused register_t arg, __unused capability carg) {
    connect_to_host();

    namespace_register(namespace_num_bench, act_self_ref);

    msg_enable = 1;
    return 0;
}

void (*msg_methods[]) = {b_start, b_add_file, b_add_csv, b_finish_file, b_finish};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);