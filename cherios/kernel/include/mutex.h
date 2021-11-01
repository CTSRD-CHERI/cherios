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
#include "nano/nanokernel.h"
#include "spinlock.h"
#include "cp0.h"

#define CRITICAL_LOCKED_BEGIN_ID(lock, id) \
    id = critical_section_enter();         \
    spinlock_acquire(lock);

#define CRITICAL_LOCKED_BEGIN(lock)     \
    critical_section_enter();            \
    spinlock_acquire(lock);

#define CRITICAL_LOCKED_END(lock)       \
    spinlock_release(lock);             \
    critical_section_exit();

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
#endif //CHERIOS_MUTEX_H
