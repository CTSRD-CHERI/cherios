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

#ifndef CHERIOS_SPINLOCK_H
#define CHERIOS_SPINLOCK_H

#include "cheric.h"

/* TODO align this nicely */
#define CACHE_LINE_SIZE 64;

typedef struct spinlock_t{
    volatile char lock;
} spinlock_t;


static inline void spinlock_init(spinlock_t* lock) {
    lock->lock = 0;
}
static inline void spinlock_acquire(spinlock_t* lock) {
    register register_t tmp;
    __asm__ volatile (
    SANE_ASM
    "1:"
            "b      2f                      \n"
            "cllb   %[tmp], %[lock]         \n"
    "3:"
            "li     $zero, 0xea1d           \n" // yield on retry
            "cllb   %[tmp], %[lock]         \n"
    "2: "
            "bnez   %[tmp], 3b              \n"
            "li     %[tmp], 1               \n"
            "cscb   %[tmp], %[tmp], %[lock] \n"
            "beqz   %[tmp], 2b              \n"
            "cllb   %[tmp], %[lock]         \n"
    : [tmp] "=r" (tmp)
    : [lock]"C"(&lock->lock)
    :
    );
}

static inline int spinlock_try_acquire(spinlock_t* lock, register_t times) {
    int result;
    register register_t tmp0;
    __asm__ volatile (
    SANE_ASM
            "li     %[result], 0                \n"
    "1:"
            "beqz   %[times], 2f                \n"
            "daddiu %[times], %[times], -1      \n"
            "cllb   %[tmp], %[lock]             \n"
    "3: "
            "bnez   %[tmp], 1b                  \n"
            "li     %[tmp], 1                   \n"
            "cscb   %[result], %[tmp], %[lock]  \n"
            "beqz   %[result], 1b               \n"
            "nop                                \n"
    "2:"
    :  [result]"=&r"(result), [tmp]"=&r"(tmp0)
    : [lock]"C"(&lock->lock), [times]"r"(times)
    :
    );

    return result;
}

static inline void spinlock_release(spinlock_t* lock) {
    lock->lock = 0;
    HW_SYNC;
}

#endif //CHERIOS_SPINLOCK_H
