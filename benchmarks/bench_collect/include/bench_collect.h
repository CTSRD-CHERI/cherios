/*-
 * Copyright (c) 2017 Lawrence Esswood
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

#ifndef CHERIOS_TEMPLATE_H
#define CHERIOS_TEMPLATE_H

#include "namespace.h"



#define MAX_Q 0x10
#define HOST_PORT 7777

#if 0

#define HOST_IP "128.232.18.57" // My PC's interface attached to the lab network

#else

#define HOST_IP "10.0.0.5" // My PC's other iterface I use to connect directly to the FPGA

#endif

static inline act_kt get_bench_collect_act(void) {
    static act_kt act = NULL;
    if(act == NULL) {
        act = namespace_get_ref(namespace_num_bench);
    }
    return act;
}

static inline int bench_start(void) {
    act_kt act = get_bench_collect_act();
    return (int)message_send(0, 0, 0, 0, NULL, NULL, NULL, NULL, act, SYNC_CALL, 0);
}

static inline void bench_add_file(size_t columns, const char* name, const char** headers) {
    act_kt act = get_bench_collect_act();
    message_send(columns, 0, 0, 0, name, headers, NULL, NULL, act, SYNC_CALL, 1);
}

static inline void bench_add_csv(const uint64_t* values, size_t nvalues) {
    act_kt act = get_bench_collect_act();
    message_send(nvalues, 0, 0, 0, values, NULL, NULL, NULL, act, SYNC_CALL, 2);
}


static inline void bench_finish_file(void) {
    act_kt act = get_bench_collect_act();
    message_send(0, 0, 0, 0, NULL, NULL, NULL, NULL, act, SYNC_CALL, 3);
}

static inline void bench_finish(void) {
    act_kt act = get_bench_collect_act();
    message_send(0, 0, 0, 0, NULL, NULL, NULL, NULL, act, SYNC_CALL, 4);
}

#endif //CHERIOS_TEMPLATE_H
