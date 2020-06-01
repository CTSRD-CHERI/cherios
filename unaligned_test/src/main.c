/*-
 * Copyright (c) 2020 Lawrence Esswood
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
#include "assert.h"
#include "string.h"
#include "stdio.h"

int main(__unused register_t arg,__unused capability carg) {

    printf("Unaligned test Hello World\n");

    char buf[200];
    bzero(buf, sizeof(buf));

    char* ptr = buf;

    if((((size_t)ptr) & 1) == 0) ptr++;

    uint32_t* ptr1 = (uint32_t*)ptr;


    int32_t* ptr2 = (int32_t*)ptr;

    uint32_t val1;

    __asm__ __volatile (
       SANE_ASM
       "li  %[v1], 0xF0F0               \n"
       "dsll %[v1], %[v1], 16           \n"
       "daddiu %[v1], %[v1], 0x7777     \n"
       : [v1]"=r"(val1)
       :
       :
    );

    int32_t val2 = -1;

    // ASM makes sure the compiler does no optimisation and so things can be put in delay slots

    uint32_t val1_out, val1_out2;
    int32_t val2_out;

    __asm__ __volatile (
        SANE_ASM
        "li  $a0, 4                 \n"
        "csw %[v1], $zero, 8(%[c1]) \n"
        "beq %[x1], %[x2], L1       \n"
        "csw %[v2], $a0, 0(%[c2])   \n"
        "teq  $zero, $zero          \n"
        "L1:                        \n"
        "clwu %[v1_out], $zero, 8(%[c1]) \n"
        "bne %[x1], %[x2], L2           \n"
        "clw  %[v2_out], $a0, 0(%[c2])  \n"
        "b L3                           \n"
        "nop                            \n"
        "L2: teq  $zero, $zero          \n"
        "L3: nop\n"
        "clwu $s1, $zero, 8(%[c1])       \n"
        "move %[v1_out2], $s1           \n"
        : [v1_out]"=r"(val1_out), [v2_out]"=r"(val2_out), [v1_out2]"=r"(val1_out2)
        : [v1]"r"(val1), [v2]"r"(val2), [x1]"r"(77), [x2]"r"(77), [c1]"C"(ptr1), [c2]"C"(ptr2)
        : "a0", "s1", "memory"
    );

    sleep(MS_TO_CLOCK(1000));

    assert_int_ex(val1, ==, val1_out);
    assert_int_ex(val1, ==, val1_out2);
    assert_int_ex(val2, ==, val2_out);

    printf("Unaligned test complete!\n");

    return 0;
}
