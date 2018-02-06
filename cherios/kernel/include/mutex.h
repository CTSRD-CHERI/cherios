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

#ifndef CHERIOS_MUTEX_H
#define CHERIOS_MUTEX_H

#include "stddef.h"
#include "mips.h"
#include "nano/nanokernel.h"
#include "spinlock.h"
#include "cp0.h"

/* TODO align this nicely */
#define CACHE_LINE_SIZE 64;

/* TODO these are now used so frequently we should make a faster version of critical enter */

extern ex_lvl_t* ex_lvl[SMP_CORES];
extern cause_t* ex_cause[SMP_CORES];

static inline uint8_t fast_critical_enter(void) {
    register uint8_t ret;
    register register_t tmp0, tmp1, tmp2;
    register capability tmp_cap;
    register capability lvl_array = ex_lvl;
    __asm__ __volatile__ (
        SANE_ASM
        "       dmfc0       %[_0], $15, 1                   \n"     // get cpu_id. might change due to interrupt.
        "1:     move        %[_2], %[_0]                    \n"     // %2 is old cpu_id
        "       andi        %[ret], %[_0], 0xff             \n"
        "       dsll        %[_1], %[ret], 5                \n"     // get offset from cpu_id
        "       clc         %[lvl], %[_1], 0(%[lvl_array])  \n"     // ex_lvl capability
        "       clld        %[_1], %[lvl]                   \n"     // load linked lvl
        "       dmfc0       %[_0], $15, 1                   \n"     // get cpu_id again. This is now the correct cpu.
        "       bne         %[_0], %[_2], 1b                \n"     // if cpu has changed, abort
        "       daddiu      %[_1], %[_1], 1                 \n"     // increment ex_lvl
        "       cscd        %[_2], %[_1], %[lvl]            \n"     // store conditional lvl. Will fail if cpu_id changed.
        "       beqz        %[_2], 1b                       \n"     // retry on failure
        "       nop                                         \n"     // nop for delay slot
        : [_2] "=r" (tmp2), [_0] "=r" (tmp0), [_1] "=r" (tmp1), [lvl] "=C" (tmp_cap), [ret] "=r" (ret), [lvl_array] "+C" (lvl_array)
        : [lvl_array] "C" (lvl_array)
        :
    );
    return ret;
}

/* Note: This must be called when ex_lvl is non zero. Until we decrement it, interrupts are soft off */
static inline void fast_critical_exit(void) {
    uint8_t cpu_id = cp0_get_cpuid();
    register register_t tmp0, tmp1, tmp2;
    tmp2 = 0;
    capability tmp_cap;
    __asm__ __volatile__ (
        SANE_ASM
        "1:"
        "       clld        %[_1], %[lvl]                   \n"     // load lvl
        "       daddiu      %[_1], %[_1], -1                \n"     // decrement level
        "       bnez        %[_1], 2f                       \n"     // skip anything with cause if still in critical lvl
        "       cld         %[_2], $zero, 0(%[ex_cause])    \n"     // load cause. If this becomes stale the sc will fail.
        "       bnez        %[_2], 3f                       \n"     // if no exception has happened just skip
        "       nop                                         \n"
        "2:"
        "       cscd        %[_2], %[_1], %[lvl]            \n"     // store. We might take an exception now (if it succeeds).
        "       beqz        %[_2], 1b                       \n"     // if store fails, try again
        "       li          %[_2], 0                        \n"
        "3:"
        : [_2] "=r" (tmp2), [_0] "=r" (tmp0), [_1] "=r" (tmp1), [lvl] "=C" (tmp_cap)
        : [lvl] "C" (ex_lvl[cpu_id]), [ex_cause] "C" (ex_cause[cpu_id])
        :
    );

    if(tmp2 != 0) {
        critical_section_exit();
    }
}

#define CRITICAL_LOCKED_BEGIN_ID(lock, id) \
    id = fast_critical_enter();         \
    spinlock_acquire(lock);

#define CRITICAL_LOCKED_BEGIN(lock)     \
    fast_critical_enter();              \
    spinlock_acquire(lock);

#define CRITICAL_LOCKED_END(lock)       \
    spinlock_release(lock);             \
    fast_critical_exit();

typedef struct semaphore_t {
    struct act_t* first_waiter;
    struct act_t* last_waiter;
    int level;
    spinlock_t lock;
} semaphore_t;

typedef struct mutex_t {
    struct act_t* owner;
    semaphore_t sem;
    size_t recursions;
} mutex_t;

void semaphore_init(semaphore_t* sem);
void semaphore_signal(semaphore_t* sem);
void semaphore_wait(semaphore_t* sem, struct act_t* waiter);
int  semaphore_try_wait(semaphore_t* sem, struct act_t* waiter);

void mutex_init(mutex_t* mu);
void mutex_release(mutex_t* mu, struct act_t* owner);
void mutex_acquire(mutex_t* mu, struct act_t* owner);
int  mutex_try_acquire(mutex_t* mu, struct act_t* owner);

void init_fast_critical_section(void);
#endif //CHERIOS_MUTEX_H
