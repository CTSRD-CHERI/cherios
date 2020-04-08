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

#include "cheric.h"
#include "pthread.h"
#include "stdio.h"
#include "assert.h"

void* start_func(void* arg) {
    return arg;
}


int main(__unused register_t arg,__unused capability carg) {
    pthread_t t1, t2, t3, t4, t5;

    // Create a bunch of threads

    pthread_create(&t1, NULL, start_func, (void*)1);
    pthread_create(&t2, NULL, start_func, (void*)2);
    pthread_create(&t3, NULL, start_func, (void*)3);
    pthread_create(&t4, NULL, start_func, (void*)4);
    pthread_create(&t5, NULL, start_func, (void*)5);

    // Join them in opposite order

    void* ret;

    pthread_join(t5, &ret);
    assert_int_ex((int)ret, ==, 5);
    pthread_join(t4, &ret);
    assert_int_ex((int)ret, ==, 4);
    pthread_join(t3, &ret);
    assert_int_ex((int)ret, ==, 3);
    pthread_join(t2, &ret);
    assert_int_ex((int)ret, ==, 2);
    pthread_join(t1, &ret);
    assert_int_ex((int)ret, ==, 1);

    printf("PThread test passes!\n");

    // Done

    return 0;
}
