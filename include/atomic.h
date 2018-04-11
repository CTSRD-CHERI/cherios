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
#ifndef CHERIOS_ATOMIC_H
#define CHERIOS_ATOMIC_H

#include "cheric.h"

#define SUF_8  "b"
#define SUF_16 "h"
#define SUF_32 "w"
#define SUF_64 "d"
#define SUF_c  "c"

#define OUT_8   "r"
#define OUT_16  "r"
#define OUT_32  "r"
#define OUT_64  "r"
#define OUT_c   "C"

#define LOAD(type)  "cll" SUF_ ## type
#define STORE(type) "csc" SUF_ ## type
#define OUT(type) "=" OUT_ ## type
#define IN(type) OUT_ ## type

#define ATOMIC_ADD(pointer, type, val, result)      \
{                                                   \
register register_t tmp;                            \
__asm__ __volatile__ (                              \
    SANE_ASM                                        \
    "1:"                                            \
    LOAD(type) "    %[out], %[ptr]          \n"     \
    "daddiu         %[tmp], %[out], %[v]    \n"     \
    STORE(type) "   %[tmp], %[tmp], %[ptr]  \n"     \
    "beqz           %[tmp], 1b              \n"     \
    "nop                                    \n"     \
: [tmp] "=r" (tmp), [out] "=r" (result)             \
: [ptr] "C" (pointer), [v] "i" (val)                \
:)    ;                                             \
}                                                   \

#define LOAD_LINK(ptr, type, result) __asm__ __volatile(LOAD(type) " %[res], %[pt]" : [res] OUT(type) (result) : [pt] IN(c) (ptr):)
#define STORE_COND(ptr, type, val, suc) __asm__ __volatile(STORE(type) " %[sc], %[vl], %[pt]" : \
                    [sc] OUT(64) (suc) : \
                    [pt] IN(c) (ptr), [vl] IN(type) (val) :)

#endif //CHERIOS_ATOMIC_H
