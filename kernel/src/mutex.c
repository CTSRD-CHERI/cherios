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

#include "mutex.h"
#include "sched.h"

void spinlock_init(spinlock_t* lock) {
    lock->lock = 0;
}
void spinlock_acquire(spinlock_t* lock) {
    __asm__ volatile (
        "start:"
        "cllb   $t0, %[lock]\n"
        "check:"
        "bnez   $t0, start\n"
        "li     $t0, 1\n"
        "cscb   $t0, $t0, %[lock]\n"
        "beqz   $t0, check\n"
        "cllb   $t0, %[lock]\n"
    :
    : [lock]"C"(lock)
    : "t0"
    );
}
void spinlock_release(spinlock_t* lock) {
    lock->lock = 0;
}

void semaphore_init(semaphore_t* sem) {
    spinlock_init(&sem->lock);
    sem->first_waiter = NULL;
    sem->last_waiter = NULL;
    sem->level = 0;
}

void semaphore_signal(semaphore_t* sem) {

    CRITICAL_LOCKED_BEGIN(&sem->lock)

    act_t* waiter = sem->first_waiter;

    if(waiter != NULL) {
        act_t* next_waiter = waiter->semaphore_next_waiter;
        sem->first_waiter = next_waiter;
        if(next_waiter == NULL) sem->last_waiter = NULL;
        CRITICAL_LOCKED_END(&sem->lock)
        sched_receives_sem_signal(waiter);
    } else {
        sem->level++;
        CRITICAL_LOCKED_END(&sem->lock)
    }
}

void semaphore_wait(semaphore_t* sem, act_t* waiter) {

    CRITICAL_LOCKED_BEGIN(&sem->lock)

    int level = sem->level;
    if(level < 0) {
        // resource unavailable
        act_t* last = sem->last_waiter;
        sem->last_waiter = waiter;
        waiter->semaphore_next_waiter = NULL;
        if(last == NULL) {
            sem->last_waiter = waiter;
        } else {
            last->semaphore_next_waiter = waiter;
        }

        sched_block(waiter, sched_sem);

        CRITICAL_LOCKED_END(&sem->lock)

        sched_reschedule(NULL, 0);

    } else {
        // resource available
        sem->level = level-1;
        CRITICAL_LOCKED_END(&sem->lock)
    }

}

int semaphore_try_wait(semaphore_t* sem, act_t* waiter) {
    int result;

    critical_section_enter();
    spinlock_acquire(&sem->lock);

    int level = sem->level;
    if(level < 0) {
        result = 0;
    } else {
        result = 1;
        sem->level = level -1;
    }

    spinlock_release(&sem->lock);
    critical_section_exit();

    return result;
}

void mutex_init(mutex_t* mu) {
    semaphore_init(&mu->sem);
    mu->owner = 0;
    mu->recursions = 0;
}

void mutex_release(mutex_t* mu, act_t* owner) {
    if(mu->owner != owner) return;

    if(mu->recursions > 0) {
        mu->recursions--;
    } else {
        mu->owner = NULL;
        semaphore_signal(&mu->sem);
    }
}

void mutex_acquire(mutex_t* mu, act_t* owner) {
    if(mu->owner == owner) {
        mu->recursions++;
    } else {
        semaphore_wait(&mu->sem, owner);
        mu->owner = owner;
    }
}

int  mutex_try_acquire(mutex_t* mu, act_t* owner) {
    if(mu->owner == owner) {
        mu->recursions++;
        return 1;
    } else {
        int res = semaphore_try_wait(&mu->sem, owner);
        if(res) {
            mu->owner = owner;
        }
        return res;
    }
}