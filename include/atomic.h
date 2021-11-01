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

#if defined(__cplusplus)
    #define RClass
#else
    #define RClass register
#endif

#define ATOMIC_ADD(pointer, type, val_type, val, result)      \
{                                                   \
RClass register_t tmp;                              \
RClass CTYPE(type) added;                           \
__asm__ __volatile__ (                              \
    SANE_ASM                                        \
    "1:"                                            \
    LOADL(type) "    %[out], %[ptr]          \n"    \
    ASMADD(type, val_type)" %[add], %[out], %[v]      \n"    \
    STOREC(type) "   %[tmp], %[add], %[ptr]  \n"    \
    "beqz           %[tmp], 1b              \n"     \
    "nop                                    \n"     \
: [tmp] "=r" (tmp), [out] CLOBOUT(type) (result), [add] CLOBOUT(type) (added) \
: [ptr] "C" (pointer), [v] IN(val_type) (val)       \
: "memory")    ;                                             \
}                                                   \


#define ATOMIC_ADD_RV(pointer, type, val_type, val) \
    ({CTYPE(type) aa_tmp; ATOMIC_ADD(pointer, type, val_type, val, aa_tmp); aa_tmp;})


#define ATOMIC_CAS(pointer, type, old_val, new_val, result) \
{                                                           \
RClass CTYPE(type) tmp;                                     \
__asm__ __volatile(                                         \
SANE_ASM                                                    \
        "1:"                                                \
LOADL(type) " %[tmp], %[ptr]            \n"                 \
BNE(type, "%[tmp]", "%[old]", "2f","%[res]") "\n"           \
"li     %[res], 0                       \n"                 \
STOREC(type) " %[res], %[newv], %[ptr]    \n"                \
"beqz   %[res], 1b                      \n"                 \
"nop                                    \n"                 \
"2:                                     \n"                 \
: [tmp] CLOBOUT(type) (tmp), [res] "=&r" (result)                     \
: [ptr] "C" (pointer), [old] IN(type) (old_val), [newv] IN(type) (new_val) \
: "memory");                                                         \
}

#define ATOMIC_SWAP(pointer, type, new_val, result) \
{                                                           \
RClass register_t tmp;                                      \
__asm__ __volatile(                                         \
SANE_ASM                                                    \
        "1:"                                                \
LOADL(type) " %[res], %[ptr]            \n"                 \
STOREC(type) " %[tmp], %[newv], %[ptr]    \n"                \
"beqz   %[tmp], 1b                      \n"                 \
"nop                                    \n"                 \
"2:                                     \n"                 \
: [tmp] "=&r" (tmp), [res] CLOBOUT(type) (result)           \
: [ptr] "C" (pointer), [newv] IN(type) (new_val)             \
: "memory");                                                \
}

#define ATOMIC_CAS_RV(pointer, type, old_val, new_val) \
    ({register_t aa_tmp; ATOMIC_CAS(pointer, type, old_val, new_val, aa_tmp); aa_tmp;})

#define ATOMIC_SWAP_RV(pointer, type, new_val) \
    ({CTYPE(type) aa_tmp; ATOMIC_SWAP(pointer, type, new_val, aa_tmp); aa_tmp;})

#define LOAD_LINK(ptr, type, result) __asm__ __volatile(LOADL(type) " %[res], %[pt]" : [res] OUT(type) (result) : [pt] IN(c) (ptr):)
#define STORE_COND(ptr, type, val, suc) __asm__ __volatile(STOREC(type) " %[sc], %[vl], %[pt]" : \
                    [sc] OUT(64) (suc) : \
                    [pt] IN(c) (ptr), [vl] IN(type) (val) : "memory")

#endif //CHERIOS_ATOMIC_H
