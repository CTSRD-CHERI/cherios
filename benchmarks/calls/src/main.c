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
#define DUP         0x10
#define SYNC_TIMES 0x3

#define DD(X) X X X X X X X X X X X X X X X X

void null_func(void) {}

typedef void void_f(void);

void nothing() {
    uint64_t start = syscall_bench_start();
    uint64_t end = syscall_bench_end();
    uint64_t start2 = syscall_bench_start();
    uint64_t end2 = syscall_bench_end();
    uint64_t diff1 = end - start;
    uint64_t diff2 = end - start;
    printf("******BENCH: Nothing: %lx, %lx\n", diff1, diff2);
}

void lib_calls(void_f* f) {

    for(int tms = 0; tms != SYNC_TIMES; tms++) {

        for(int i = 0; i != 32; i++) {
            f();
        }

        uint64_t start = syscall_bench_start();

        for(int i = 0; i != SYNC_SAMPLES/DUP; i++) {
            DD(f();)
        }

        size_t end = syscall_bench_end();

        uint64_t diff2 = end - start;

        printf("******BENCH: call %x of %x (x%x) : %lx\n", tms+1, SYNC_TIMES, SYNC_SAMPLES, diff2);
    }

    return;
}

extern CROSS_DOMAIN(dummy_lib_f);
extern TRUSTED_CROSS_DOMAIN(dummy_lib_f);
void dummy_lib_f(void) {
    return;;
}


#define BENCH_EXT_LIST(ITEM, ...)           \
    ITEM(f1, void, (void), __VA_ARGS__)     \
    ITEM(f2, void, (void), __VA_ARGS__)

PLT(bench_t, BENCH_EXT_LIST)
PLT_ALLOCATE(bench_t, BENCH_EXT_LIST)

int main(register_t arg, capability carg) {

    get_ctl()->cds = get_type_owned_by_process();
    get_ctl()->cdl = &entry_stub;

    bench_t bench;
    bench.f1 = cheri_seal(&dummy_lib_f, get_ctl()->cds);

            // should be cheri_seal(&TRUSTED_CROSS_DOMAIN(dummy_lib_f), get_ctl()->cds) but there is a compiler bug
    bench.f2 = cheri_seal(&CROSS_DOMAIN(dummy_lib_f), get_ctl()->cds);

    capability data_arg = cheri_seal(get_ctl(), get_ctl()->cds);

    init_bench_t(&bench, data_arg, &plt_common_trusting);

    nothing();

    // Test nano call

    printf("Nano:");
    lib_calls(&nano_dummy); // nano

    // Test lib trusting

    printf("Trusting, trusted:");
    lib_calls(&f1); // trusting, trusted
    printf("Trusting, untrusted:");
    lib_calls(&f2); // trusting, untrusted

    init_bench_t_change_mode(&plt_common_untrusting);

    printf("Untrusting, trusted:");
    lib_calls(&f1); // untrusting, trusted
    printf("Untrusting, untrusted:");
    lib_calls(&f2); // untrusting, untrusted
}

void (*msg_methods[]) = {};

size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);