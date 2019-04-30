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

#define SYNC_SAMPLES 0x1000

void null_func(void) {}


void send_rec(register_t reg, capability cap) {
    msg_entry(0);
}

int main(register_t arg, capability carg) {
    // Test sync send
    thread t = thread_new("sync_send_rec", 0, NULL, &send_rec);

    act_kt sync_act = syscall_act_ctrl_get_ref(get_control_for_thread(t));

    // warm up

    for(int i = 0; i != 32; i++) {
        message_send(0, 0, 0, 0, NULL, NULL, NULL, NULL, sync_act, SYNC_CALL, 0);
    }

    uint64_t start = syscall_bench_start();
    uint64_t end = syscall_bench_end();

    uint64_t diff1 = end - start;

    start = syscall_bench_start();

    for(int i = 0; i != SYNC_SAMPLES; i++) {
        message_send(0, 0, 0, 0, NULL, NULL, NULL, NULL, sync_act, SYNC_CALL, 0);
    }

    HW_TRACE_ON;
    message_send(0, 0, 0, 0, NULL, NULL, NULL, NULL, sync_act, SYNC_CALL, 0);
    HW_TRACE_OFF;

    end = syscall_bench_end();

    uint64_t diff2 = end - start;

    printf("******BENCH: Nothing: %lx\n", diff1);
    printf("******BENCH: SyncSend x " STRINGIFY(SYNC_SAMPLES) ": %lx\n", diff2);
}

void (*msg_methods[]) = {null_func};

size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);