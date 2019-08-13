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
#include "bench_collect.h"
#include "temporal.h"
#include "assert.h"

#define SYNC_SAMPLES 0x1000
#define SYNC_TIMES 1000
#define COLUMNS 2

uint64_t vals[COLUMNS*SYNC_TIMES];

void null_func(void) {}


void send_rec(__unused register_t reg, __unused capability cap) {
    __unused size_t n = syscall_provide_sync(cap);
    assert(n > (2*SYNC_TIMES));
    msg_entry(-1, 0);
}

void become_untrust(capability sealer) {
    get_ctl()->cds = sealer;
    get_ctl()->cdl = &entry_stub;
    init_kernel_if_t_change_mode(&plt_common_untrusting);
}

void reset() {
    syscall_next_sync();
    replace_usp();
}

void send(act_kt sync_act, uint64_t* column) {

    for(int tms = 0; tms != SYNC_TIMES; tms++) {

        // Make sure we have enough tokens

        if(tms % 100 == 99) sleep(MS_TO_CLOCK(3000));
        reset();
        // replace usp on other side
        message_send(0, 0, 0, 0, get_ctl()->cds, NULL,NULL, NULL, sync_act, SYNC_CALL, 2);
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

        *column = diff2;
        column += COLUMNS;
#if (!GO_FAST)
        printf("******BENCH: SyncSend %x of %x (x%x) : %lx\n", tms+1, SYNC_TIMES, SYNC_SAMPLES, diff2);
#endif
    }

    return;
}

int main(__unused register_t arg, __unused capability carg) {
    // Test sync send


    thread t = thread_new("sync_send_rec", 0, cap_malloc((CAP_SIZE * 5) * SYNC_TIMES), &send_rec);

    act_kt sync_act = syscall_act_ctrl_get_ref(get_control_for_thread(t));

    syscall_provide_sync(cap_malloc((CAP_SIZE * 5) * SYNC_TIMES));

    bench_start();

    const char * hdrs[] = { "Trusting("X_STRINGIFY(SYNC_SAMPLES)")",
                            "Untrusting("X_STRINGIFY(SYNC_SAMPLES)")"};

    bench_add_file(COLUMNS, "messages.csv", hdrs);

    // Test sending
    send(sync_act, vals+0);

    // become distrusting
    become_untrust(get_type_owned_by_process());

    message_send(0, 0, 0, 0, get_ctl()->cds, NULL,NULL, NULL, sync_act, SYNC_CALL, 1);

    send(sync_act,vals+1);

    bench_add_csv(vals, SYNC_TIMES * COLUMNS);

    bench_finish();

    return 0;
}

void (*msg_methods[]) = {null_func, become_untrust, reset};

size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);