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
#include "macroutils.h"
#include "temporal.h"
#include "lightweight_ccall.h"

#define SYNC_SAMPLES 0x1000
#define DUP         0x10
#define SYNC_TIMES 1000

#define COLUMNS 10

#define DD(X) X X X X X X X X X X X X X X X X

void null_func(void) {}

typedef void void_f(void);

uint64_t vals[COLUMNS*SYNC_TIMES];

void nothing(uint64_t* column) {
    uint64_t start = syscall_bench_start();
    uint64_t end = syscall_bench_end();
    uint64_t start2 = syscall_bench_start();
    uint64_t end2 = syscall_bench_end();
    uint64_t diff1 = end - start;
    uint64_t diff2 = end2 - start2;
#if (!GO_FAST)
    printf("******BENCH: Nothing: %lx, %lx\n", diff1, diff2);
#endif
    column[0] = diff1;
    column[COLUMNS] = diff2;
}

__attribute__((noinline)) void lib_calls(void_f* f, uint64_t* column) {

    for(int tms = 0; tms != SYNC_TIMES; tms++) {

        // Calling may require the unsafe stack. Consume it.
        if(tms % 100 == 99) sleep(MS_TO_CLOCK(3000));
        replace_usp();

        for(int i = 0; i != 32; i++) {
            f();
        }

        uint64_t start = syscall_bench_start();

        for(int i = 0; i != SYNC_SAMPLES/DUP; i++) {
            DD(f();)
        }

        size_t end = syscall_bench_end();

        uint64_t diff2 = end - start;

        *column = diff2;
        column+=COLUMNS;

#if (!GO_FAST)
        printf("******BENCH: call %x of %x (x%x) : %lx\n", tms+1, SYNC_TIMES, SYNC_SAMPLES, diff2);
#endif
    }

    return;
}

__attribute__((noinline)) void lib_calls_leaf(void_f* f, capability  data, uint64_t* column) {

    for(int tms = 0; tms != SYNC_TIMES; tms++) {

        // Calling may require the unsafe stack. Consume it.
        if(tms % 100 == 99) sleep(MS_TO_CLOCK(3000));

        for(int i = 0; i != 32; i++) {
            LIGHTWEIGHT_CCALL_FUNC(v, f, data, 0, 0);
        }

        uint64_t start = syscall_bench_start();

        for(int i = 0; i != SYNC_SAMPLES/DUP; i++) {
            DD(LIGHTWEIGHT_CCALL_FUNC(v, f, data, 0, 0);)
        }

        size_t end = syscall_bench_end();

        uint64_t diff2 = end - start;

        *column = diff2;
        column+=COLUMNS;

#if (!GO_FAST)
        printf("******BENCH: call %x of %x (x%x) : %lx\n", tms+1, SYNC_TIMES, SYNC_SAMPLES, diff2);
#endif
    }

    return;
}

extern void CROSS_DOMAIN(dummy_lib_f)(void);
extern void TRUSTED_CROSS_DOMAIN(dummy_lib_f)(void);
__used void dummy_lib_f(void) {
    return;;
}


#define BENCH_EXT_LIST(ITEM, ...)           \
    ITEM(f1, void, (void), __VA_ARGS__)     \
    ITEM(f2, void, (void), __VA_ARGS__)

PLT(bench_t, BENCH_EXT_LIST)
PLT_ALLOCATE(bench_t, BENCH_EXT_LIST)


// A CTL and stack for the "compartment" we all calling into. Its really the same one but it needs its own CTL to be a compartment
CTL_t fake_ctl;
capability fake_stack[10];

int main(__unused register_t arg, __unused capability carg) {

    // Actually sends a lot of messages in order to create new stacks
    syscall_provide_sync(cap_malloc((CAP_SIZE * 13) * SYNC_TIMES));

    get_ctl()->cds = get_type_owned_by_process();
    get_ctl()->cdl = &entry_stub;

    // Start new benchmark
    bench_start();

    const char * hdrs[] = {"Nothing",
              "Nano Calls("X_STRINGIFY(SYNC_SAMPLES)")",
               "Normal Calls("X_STRINGIFY(SYNC_SAMPLES)")",
               "Leaf Calls("X_STRINGIFY(SYNC_SAMPLES)")",
               "CTrusting/Trusted("X_STRINGIFY(SYNC_SAMPLES)")",
               "CTrusting/Untrusted("X_STRINGIFY(SYNC_SAMPLES)")",
             "Trusting/Trusted("X_STRINGIFY(SYNC_SAMPLES)")",
             "Trusting/Untrusted("X_STRINGIFY(SYNC_SAMPLES)")",
             "Untrusting/Trusted("X_STRINGIFY(SYNC_SAMPLES)")",
             "Untrusting/Untrusted("X_STRINGIFY(SYNC_SAMPLES)")"};

    bench_add_file(COLUMNS, "calls.csv", hdrs);

    bench_t bench;
    bench.f1 = cheri_seal(&dummy_lib_f, get_ctl()->cds);

            // should be cheri_seal(&TRUSTED_CROSS_DOMAIN(dummy_lib_f), get_ctl()->cds) but there is a compiler bug
    bench.f2 = cheri_seal(&CROSS_DOMAIN(dummy_lib_f), get_ctl()->cds);

    fake_ctl = *get_ctl();
    fake_ctl.csp = fake_stack + (sizeof(fake_stack) / sizeof(capability));
    fake_ctl.guard.guard = (uint64_t)callable_ready;
    capability data_arg = cheri_seal(&fake_ctl, get_ctl()->cds);

    init_bench_t(&bench, data_arg, &plt_common_trusting);

    nothing(vals);

    // Test nano call

    printf("Nano:\n");
    sleep(MS_TO_CLOCK(100));
    syscall_printf("BP should be : %lx\n", (size_t)&lib_calls);

    lib_calls(&nano_dummy,vals+1); // nano

    // Test call normal

    printf("Normal:\n");
    sleep(MS_TO_CLOCK(100));

    lib_calls(&dummy_lib_f, vals+2);

    // Test call leaf (don't bother with attestation bit

    printf("Leaf:\n");
    sleep(MS_TO_CLOCK(100));

    lib_calls_leaf(bench.f1, data_arg, vals + 3);

    // Test lib trusting

    init_bench_t_change_mode(&plt_common_complete_trusting);

    printf("CTrusting, trusted:\n");
    sleep(MS_TO_CLOCK(100));

    lib_calls(&f1, vals+4); // trusting, trusted

    printf("CTrusting, untrusted:\n");
    sleep(MS_TO_CLOCK(100));
    lib_calls(&f2, vals+5); // trusting, untrusted

    init_bench_t_change_mode(&plt_common_trusting);

    printf("Trusting, trusted:\n");
    sleep(MS_TO_CLOCK(100));
    lib_calls(&f1, vals+6); // trusting, trusted
    printf("Trusting, untrusted:\n");
    sleep(MS_TO_CLOCK(100));
    lib_calls(&f2, vals+7); // trusting, untrusted

    init_bench_t_change_mode(&plt_common_untrusting);

    printf("Untrusting, trusted:\n");
    sleep(MS_TO_CLOCK(100));
    lib_calls(&f1, vals + 8); // untrusting, trusted
    printf("Untrusting, untrusted:\n");
    sleep(MS_TO_CLOCK(100));
    lib_calls(&f2, vals + 9); // untrusting, untrusted

    bench_add_csv(vals, SYNC_TIMES * COLUMNS);

    bench_finish();

    return 0;
}

void (*msg_methods[]) = {};

size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);