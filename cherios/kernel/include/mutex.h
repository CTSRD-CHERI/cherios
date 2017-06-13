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

/* TODO align this nicely */
#define CACHE_LINE_SIZE 64;

/* TODO these are now used so frequently we should make a faster version of critical enter */

extern ex_lvl_t* ex_lvl;
extern cause_t* ex_cause;

/* This first one may need to be atomix. The second will be guarded by the first */
#define FAST_CRITICAL_ENTER (*ex_lvl)++;
#define FAST_CRITICAL_EXIT (*ex_lvl)--; if(*ex_lvl == 0 && *ex_cause != 0) {*ex_lvl = 1; critical_section_exit();}

#define CRITICAL_LOCKED_BEGIN(lock)     \
    FAST_CRITICAL_ENTER                 \
    spinlock_acquire(lock);

#define CRITICAL_LOCKED_END(lock)       \
    spinlock_release(lock);             \
    FAST_CRITICAL_EXIT

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
