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
#include "unistd.h"
#include "mman.h"

#define PING_DUMP_PORT 12345


static void dump_tracking(struct revoke_tracking* tracking) {
    printf("Virt alloc, virt free, revoke start, revoke finish, revoke bytes\n %lx, %lx, %ld, %ld, %lx\n",
            tracking->total_virt_alloc,
            tracking->total_virt_free,
            tracking->revokes_started,
            tracking->revokes_finished,
            tracking->revoked_bytes);
}

static void dump_all_mmap(void) {
    mdump();
}

static void dump_stacks(void) {

    for(act_control_kt act = syscall_actlist_first(); act != NULL ; act = syscall_actlist_next(act)) {
        user_stats_t* stats = syscall_act_user_info_ref(act);
        const char* name = syscall_get_name(syscall_act_ctrl_get_ref(act));

        printf("%s. reqs: %ld. depth %ld. More:\n", name, stats->temporal_reqs,stats->temporal_depth);
        for(size_t i = 0; i != EXTRA_TEMPORAL_TRACKING; i++) {
            if(stats->stacks_at_level[i] == 0) break;
            printf("%d\n", stats->stacks_at_level[i]);
        }

    }
}

int main(__unused register_t arg, __unused capability carg) {

    // Set up a TCP server

    struct tcp_bind bind;
    bind.addr.addr = IP_ADDR_ANY->addr;
    bind.port = PING_DUMP_PORT;
    listening_token_or_er_t token_or_er = netsock_listen_tcp(&bind, 1, NULL, NULL);

    assert(IS_VALID(token_or_er));

    __unused listening_token tok = token_or_er.val;


    // Some setup for what this can do


    struct revoke_tracking* tracking = get_tracking();

    while(1) {
        // Accept and spawn a new thread
        NET_SOCK ns = netsock_accept(0);

        char buf[2];

        read(&ns->sock, buf, 2);

        assert(buf[1] == '\n');

        switch(buf[0]) {
            case 't':
                dump_tracking(tracking);
                break;
            case 'm':
                dump_all_mmap();
                break;
            case 's':
                dump_stacks();
                break;
            default:
                printf("Ping dump got a command it did not unserstand\n");
        }

        close(&ns->sock);
    }
}
