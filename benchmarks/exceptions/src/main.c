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

#include <exceptions.h>
#include "cheric.h"
#include "thread.h"
#include "syscalls.h"
#include "msg.h"
#include "stdio.h"
#include "capmalloc.h"
#include "dylink.h"
#include "bench_collect.h"

#define SYNC_SAMPLES 0x1000
#define SYNC_TIMES 0x3
#define COLUMNS 2

uint64_t vals[COLUMNS*SYNC_TIMES];

// handler. Bump $a0, then return
__asm__ (
    SANE_ASM
    ".weak exception_return_dummy\n"
    ".text\n"
    ".global bench_exp_handle\n"
    "bench_exp_handle:\n"
    "clcbi       $c1, "X_STRINGIFY(CTLP_OFFSET_CGP)"($idc)            \n"
    "clcbi       $idc, %captab20(nano_kernel_if_t_data_obj)($c1)    \n"
    "clcbi       $c1, %capcall20(exception_return_dummy)($c1)       \n"
    "daddiu      $a0, $a0, 1                                        \n"
    "ccall       $c1, $idc, 2                                       \n"
    "nop                                                            \n"
);

extern void bench_exp_handle(void);

int chandleint(__unused register_t cause, __unused register_t ccause, exception_restore_frame* restore_frame) {
    restore_frame->mf_a0++;
    return 0;
}

void do_test(uint64_t* column) {
    for(int tms = 0; tms != SYNC_TIMES; tms++) {

        // warm up

#define TRAP_X(X)                           \
        __asm__  __volatile(                \
                "li $a1, %[limit] \n"       \
                "li $a0, 0        \n"       \
                "tne $a0, $a1     \n"       \
                :                           \
                : [limit]"i"(X)             \
                : "a0"                      \
                );

        TRAP_X(8);

        uint64_t start = syscall_bench_start();

        // for real

        TRAP_X(SYNC_SAMPLES);

        size_t end = syscall_bench_end();

        uint64_t diff2 = end - start;

        *column = diff2;

        column += COLUMNS;

        printf("******BENCH: call %x of %x (x%x) : %lx\n", tms+1, SYNC_TIMES, SYNC_SAMPLES, diff2);
    }

    return;
}


int main(__unused register_t arg, __unused capability carg) {

    bench_start();

    const char * hdrs[] = {"ASM("X_STRINGIFY(SYNC_SAMPLES)")", "C("X_STRINGIFY(SYNC_SAMPLES)")"};

    bench_add_file(COLUMNS, "exceptions.csv", hdrs);

    register_exception_raw(&bench_exp_handle, get_ctl());

    do_test(vals + 0);

    register_vectored_exception(&chandleint, MIPS_CP0_EXCODE_TRAP);

    do_test(vals + 1);

    bench_add_csv(vals, COLUMNS * SYNC_TIMES);

    bench_finish();

    return 0;
}

void (*msg_methods[]) = {};

size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);