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

#include "pthread.h"
#include "errno.h"
#include "atomic.h"
#include "assert.h"

// A thread can only be waiting on a single mutex, so allocate space for the chain here

__thread notify_queue_t own_link;

int pthread_mutex_init(pthread_mutex_t *mutex, __unused const pthread_mutexattr_t *attr) {
    mutex->owner_id = 0;
    mutex->lock_count = 0;
    mutex->head = NULL;
    return 0;
}

int pthread_mutex_destroy(__unused pthread_mutex_t *mutex) {
    // nop
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {

    //Handle recursion

    uint64_t own_id = (uint64_t)act_self_notify_ref;

    if(mutex->owner_id == own_id) {
        mutex->lock_count++;
        return 0;
    }

    HW_SYNC;

    if(mutex->owner_id) return EBUSY;

    int result;

    // Claim ownership

    result = ATOMIC_CAS_RV(&mutex->owner_id, 64, 0, own_id);

    // If this works fill in other fields

    if(result) {
        mutex->lock_count = 1;
        return 0;
    }

    return EBUSY;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {

    // Handle recursion

    do {

        uint64_t own_id = (uint64_t)act_self_notify_ref;

        if (mutex->owner_id == own_id) {
            mutex->lock_count++;
            return 0;
        }

        while (mutex->owner_id == 0) {
            if (pthread_mutex_trylock(mutex) == 0) return 0;
        }

        // Tentatively, the mutex is locked. Add self to queue.

        notify_queue_t *link = &own_link;
        link->notify_me = act_self_notify_ref;

        int result;

        do {
            notify_queue_t *old = mutex->head;
            link->next = old;
            result = ATOMIC_CAS_RV(&mutex->head, c, old, link);
        } while (result == 0);

        // We are now in the queue, if the mutex is subsequently unlocked we will be woken.
        // However, the mutex may ALREADY have been freed, and if we are the head, we should remove ourself and try again.

        notify_queue_t *head_copy = mutex->head;

        if (mutex->owner_id == 0 && head_copy == link) {
            notify_queue_t *next = link->next;
            result = ATOMIC_CAS_RV(&mutex->head, c, link, next);
            if (result) continue; // Removed self from head
            // Otherwise we are either deeper in the queue or something else removed us.
        }

        while(mutex->owner_id != own_id) {
            syscall_cond_wait(0, 0);
        }

    } while(1);

}



int pthread_mutex_unlock(pthread_mutex_t * mutex) {
    assert(mutex->owner_id == (uint64_t)act_self_notify_ref);

    if(--mutex->lock_count == 0) {
        // Try POP
        notify_queue_t* hd_copy = mutex->head;

        act_notify_kt next_owner = NULL;

        while(hd_copy) {
            notify_queue_t* next = hd_copy->next;
            int result = ATOMIC_CAS_RV(&mutex->head, c, hd_copy, next);
            if(result) {
                // We have extracted the next owner.
                next_owner = hd_copy->notify_me;
                break;
            }
            hd_copy = mutex->head;
        }

        mutex->owner_id = (uint64_t)next_owner;

        HW_SYNC;

        if(next_owner) {
            syscall_cond_notify(next_owner);
        }
    }

    return 0;
}

//int   pthread_mutex_getprioceiling(const pthread_mutex_t *, int *);
//int   pthread_mutex_setprioceiling(pthread_mutex_t *, int, int *);