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
#include "syscalls.h"
#include "string.h"
#include "stdio.h"
#include "errno.h"
#include "stddef.h"
#include "stdlib.h"

__thread pthread_wrapper* self_wrapper;

int pthread_attr_init(pthread_attr_t *attr) {
    static int n = 0;
    memcpy(attr->name, syscall_get_name(act_self_ref), ACT_NAME_MAX_LEN);
    snprintf(attr->name+ACT_NAME_MAX_LEN-3,3,"%02d",n++);
    return 0;
}

int pthread_attr_destroy(__unused pthread_attr_t *attr) {
    // Nop as init does no allocation
    return 0;
}

__dead2
void pthread_exit(void * retval) {
    self_wrapper->retval = retval;
    condition_set_and_notify(&self_wrapper->cond, 1, &self_wrapper->notify);
    object_destroy();
}

__dead2
static inline void start_pthread(__unused register_t arg, capability carg) {
    pthread_wrapper* wrapper = (pthread_wrapper*)carg;
    self_wrapper = wrapper;

    void* retval = wrapper->start(wrapper->arg);
    pthread_exit(retval);
}

int pthread_create(pthread_t *pthread, const pthread_attr_t * attr, void *(*start)(void *) , void *arg) {

    pthread_attr_t local_attr;
    int used_local = 0;

    if(attr == NULL) {
        pthread_attr_init(&local_attr);
        attr = &local_attr;
        used_local = 1;
    }

    pthread_wrapper* wrapper = (pthread_wrapper*)malloc(sizeof(pthread_wrapper));

    wrapper->arg = arg;
    wrapper->start = start;

    // We coerce the function start to take one few arguments.
    // This is safe as we pass the (ignored) argument as a register.
    // If we were stack passing a shim would be needed.
    thread t = thread_new(attr->name, 0, wrapper, &start_pthread);

    if(used_local) {
        pthread_attr_destroy(&local_attr);
    }

    if(t != NULL) {

        wrapper->t = t;

        *pthread = wrapper;

        return 0;
    }

    free(wrapper);

    return EAGAIN;
}

int pthread_join(pthread_t pthread, void **retval) {
    int res = condition_sleep_for_equal(&pthread->notify, &pthread->closed, &pthread->cond, 1, 0, act_self_notify_ref);

    if(retval != NULL) {
        *retval = ((int)res == CONDITION_CANCELLED) ? PTHREAD_CANCELLED : pthread->retval;
    }

    // FIXME: Cross thread free not currently functional. This is dangerous.
    free(pthread);

    return 0;
}
