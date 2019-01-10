/*-
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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

#ifndef CHERIOS_USERNANO_H
#define CHERIOS_USERNANO_H

#include "nanokernel.h"
#include "cheric.h"
#include "macroutils.h"

#define GET_NANO_SYSCALL(c1,c2,n)                   \
capability c1, c2;                                  \
__asm__ (                                           \
"li     $a0, 0          \n"                         \
        "li     $a1, %[i]       \n"                 \
        "syscall                \n"                 \
        "cmove  %[_foo_c1], $c1      \n"            \
        "cmove  %[_foo_c2], $c2      \n"            \
:   [_foo_c1]"=C"(c1), [_foo_c2]"=C"(c2)            \
:   [i]"i"(n)                                       \
: "a0", "a1", "$c1", "$c2");                        \

/* Assuming you trust your memory (i.e. are secure loaded) you can call this to populate your nano kernel if and
 * then use the normal interface rather than having to use syscall repeatedly */

static inline void init_nano_if_sys(common_t* mode) {
    nano_kernel_if_t interface;
    size_t limit = N_NANO_CALLS;
    capability data;
    __asm__(
            SANE_ASM
            "li $a0, 0                      \n"
            "li $a1, 0                      \n"
            "li $t0, 0                      \n"
            "1:syscall                      \n"
            "csc  $c1, $t0, 0(%[ifc])       \n"
            "daddiu $a1, $a1, 1             \n"
            "bne $a1, %[total], 1b          \n"
            "daddiu $t0, $t0, " CAP_SIZE_S "\n"
            "cmove  %[d], $c2               \n"
    : [d]"=C"(data)
    : [ifc]"C"(&interface),[total]"r"(limit)
    : "a0", "a1", "t0", "$c1", "$c2"
    );

    init_nano_kernel_if_t(&interface, data);
}
#endif //CHERIOS_USERNANO_H
