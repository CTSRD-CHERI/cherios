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

#include "syscalls.h"

capability compact_code(__unused capability segment_table, __unused capability start, __unused capability end,
                        __unused capability code_seg_write, __unused register_t code_seg_offset, __unused register_t flags,
                        capability ret) {
    return ret;
}

__attribute__((noreturn))
void lw_panic(void) {
    syscall_panic();
    for(;;);
}

void __assert(__unused const char *assert_function, __unused const char *assert_file,
              __unused int assert_lineno, __unused const char *assert_message) {
    syscall_puts("(lw) assertion failure at ");
    syscall_puts(assert_function);
    syscall_puts(":");
    syscall_puts(assert_message);
    lw_panic();
}

void __assert_int_ex(__unused const char *assert_function, __unused const char *assert_file,
                     __unused int assert_lineno, __unused const char *am, __unused const char *opm, __unused const char *bm,
                     __unused unsigned long long int a, __unused unsigned long long int b) {
    lw_panic();
}
