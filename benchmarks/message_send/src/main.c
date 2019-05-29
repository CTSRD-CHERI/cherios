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
#include "thread.h"
#include "syscalls.h"
#include "msg.h"
#include "stdio.h"
#include "capmalloc.h"

#define SYNC_SAMPLES 0x1000
#define SYNC_TIMES 0x10

void null_func(void) {}


void send_rec(__unused register_t reg, __unused capability cap) {
    msg_entry(-1, 0);
}

void become_untrust(capability sealer) {
    get_ctl()->cds = sealer;
    get_ctl()->cdl = &entry_stub;
    init_kernel_if_t_change_mode(&plt_common_untrusting);
}

void nothing() {
    uint64_t start = syscall_bench_start();
    uint64_t end = syscall_bench_end();

    uint64_t diff1 = end - start;

    printf("******BENCH: Nothing: %lx\n", diff1);
}

void send(act_kt sync_act) {

    nothing();

    for(int tms = 0; tms != SYNC_TIMES; tms++) {

        // Make sure we have enough tokens

        syscall_next_sync();

        // warm up

        for(int i = 0; i != 32; i++) {
            message_send(0, 0, 0, 0, NULL, NULL, NULL, NULL, sync_act, SYNC_CALL, 0);
        }

        uint64_t start = syscall_bench_start();

        for(int i = 0; i != SYNC_SAMPLES; i++) {
            message_send(0, 0, 0, 0, NULL, NULL, NULL, NULL, sync_act, SYNC_CALL, 0);
        }

        size_t end = syscall_bench_end();

        uint64_t diff2 = end - start;

        printf("******BENCH: SyncSend %x of %x (x%x) : %lx\n", tms+1, SYNC_TIMES, SYNC_SAMPLES, diff2);
    }

    return;
}

int main(__unused register_t arg, __unused capability carg) {
    // Test sync send


    thread t = thread_new("sync_send_rec", 0, NULL, &send_rec);

    act_kt sync_act = syscall_act_ctrl_get_ref(get_control_for_thread(t));

    syscall_provide_sync(cap_malloc(0x1000));

    // Test sending
    send(sync_act);

    // become distrusting
    become_untrust(get_type_owned_by_process());

    message_send(0, 0, 0, 0, get_ctl()->cds, NULL,NULL, NULL, sync_act, SYNC_CALL, 1);

    send(sync_act);

    return 0;
}

void (*msg_methods[]) = {null_func, become_untrust};

size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);