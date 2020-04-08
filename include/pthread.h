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

#ifndef CHERIOS_PTHREAD_H
#define CHERIOS_PTHREAD_H

// The native CheriOS API is in thread.h, this is a POSIX wrapper.

#include "thread.h"
#include "sys/types.h"
#include "object.h"
#include "condition.h"
#include "sys/time.h"

#define sched_yield() sleep(0)

#define PTHREAD_CANCELLED ((void*)-1)

// Only start/join are currently implemented. Sync is still a WIP

typedef condition_t pthread_cond_t;
typedef int pthread_condattr_t;
typedef int pthread_key_t;
typedef int pthread_mutexattr_t;
typedef int pthread_once_t;
typedef int pthread_rwlock_t;
typedef int pthread_rwlockattr_t;

struct sched_param;
struct notify_queue;

typedef struct notify_queue {
    // Probably need a better queue structure here
    act_notify_kt notify_me;
    struct notify_queue* next;
} pthread_mutex_t;

typedef struct {
    thread t;
    void* retval;
    void* arg;
    void *(*start)(void *);
    act_notify_kt notify; // Notify this one on exit
    cancel_t closed;
    condition_t cond;
} pthread_wrapper;

typedef pthread_wrapper* pthread_t;

extern __thread pthread_wrapper* self_wrapper;

typedef struct {
    char name[ACT_NAME_MAX_LEN];
} pthread_attr_t;

// Implemented

__BEGIN_DECLS

int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
__dead2
void pthread_exit(void * retval);
int pthread_create(pthread_t *pthread, const pthread_attr_t * attr, void *(*start)(void *) , void *arg);
int pthread_join(pthread_t pthread, void **retval);

// Not implemented

int   pthread_attr_getdetachstate(const pthread_attr_t *, int *);
int   pthread_attr_getguardsize(const pthread_attr_t *, size_t *);
int   pthread_attr_getinheritsched(const pthread_attr_t *, int *);
int   pthread_attr_getschedparam(const pthread_attr_t *, struct sched_param *);
int   pthread_attr_getschedpolicy(const pthread_attr_t *, int *);
int   pthread_attr_getscope(const pthread_attr_t *, int *);
int   pthread_attr_getstackaddr(const pthread_attr_t *, void **);
int   pthread_attr_getstacksize(const pthread_attr_t *, size_t *);
int   pthread_attr_setdetachstate(pthread_attr_t *, int);
int   pthread_attr_setguardsize(pthread_attr_t *, size_t);
int   pthread_attr_setinheritsched(pthread_attr_t *, int);
int   pthread_attr_setschedparam(pthread_attr_t *, const struct sched_param *);
int   pthread_attr_setschedpolicy(pthread_attr_t *, int);
int   pthread_attr_setscope(pthread_attr_t *, int);
int   pthread_attr_setstackaddr(pthread_attr_t *, void *);
int   pthread_attr_setstacksize(pthread_attr_t *, size_t);
int   pthread_cancel(pthread_t);
void pthread_cleanup_push(void (*)(void *), void *);
void  pthread_cleanup_pop(int);
int   pthread_cond_broadcast(pthread_cond_t *);
int   pthread_cond_destroy(pthread_cond_t *);
int   pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int   pthread_cond_signal(pthread_cond_t *);
int   pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
int   pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int   pthread_condattr_destroy(pthread_condattr_t *);
int   pthread_condattr_getpshared(const pthread_condattr_t *, int *);
int   pthread_condattr_init(pthread_condattr_t *);
int   pthread_condattr_setpshared(pthread_condattr_t *, int);
int   pthread_detach(pthread_t);
int   pthread_equal(pthread_t, pthread_t);
int   pthread_getconcurrency(void);
int   pthread_getschedparam(pthread_t, int *, struct sched_param *);
void *pthread_getspecific(pthread_key_t);
int   pthread_key_create(pthread_key_t *, void (*)(void *));
int   pthread_key_delete(pthread_key_t);
int   pthread_mutex_destroy(pthread_mutex_t *);
int   pthread_mutex_getprioceiling(const pthread_mutex_t *, int *);
int   pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int   pthread_mutex_lock(pthread_mutex_t *);
int   pthread_mutex_setprioceiling(pthread_mutex_t *, int, int *);
int   pthread_mutex_trylock(pthread_mutex_t *);
int   pthread_mutex_unlock(pthread_mutex_t *);
int   pthread_mutexattr_destroy(pthread_mutexattr_t *);
int   pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_getprotocol(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_getpshared(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_gettype(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_init(pthread_mutexattr_t *);
int   pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
int   pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int   pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
int   pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int   pthread_once(pthread_once_t *, void (*)(void));
int   pthread_rwlock_destroy(pthread_rwlock_t *);
int   pthread_rwlock_init(pthread_rwlock_t *, const pthread_rwlockattr_t *);
int   pthread_rwlock_rdlock(pthread_rwlock_t *);
int   pthread_rwlock_tryrdlock(pthread_rwlock_t *);
int   pthread_rwlock_trywrlock(pthread_rwlock_t *);
int   pthread_rwlock_unlock(pthread_rwlock_t *);
int   pthread_rwlock_wrlock(pthread_rwlock_t *);
int   pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
int   pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *, int *);
int   pthread_rwlockattr_init(pthread_rwlockattr_t *);
int   pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);
pthread_t pthread_self(void);
int   pthread_setcancelstate(int, int *);
int   pthread_setcanceltype(int, int *);
int   pthread_setconcurrency(int);
int   pthread_setschedparam(pthread_t, int , const struct sched_param *);
int   pthread_setspecific(pthread_key_t, const void *);
void  pthread_testcancel(void);

__END_DECLS

#endif //CHERIOS_PTHREAD_H
