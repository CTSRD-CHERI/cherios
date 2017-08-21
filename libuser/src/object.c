/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "mips.h"
#include "object.h"
#include "cheric.h"
#include "assert.h"
#include "namespace.h"
#include "queue.h"
#include "syscalls.h"
#include "string.h"
#include "stdio.h"
#include "nano/nanokernel.h"
#include "mman.h"

__thread act_control_kt act_self_ctrl = NULL;
__thread act_kt act_self_ref  = NULL;
__thread queue_t * act_self_queue = NULL;

kernel_if_t kernel_if;

ALLOCATE_PLT_SYSCALLS
ALLOCATE_PLT_NANO

#define MAX_SIMPLE_ALLOC 0x8000 - (2 * RES_META_SIZE)

res_t res_pool;
size_t pool_remains;

void object_init(act_control_kt self_ctrl, queue_t * queue, kernel_if_t* kernel_if_c) {

    /* This comes in null for threads other than the first - the interface is not thread local */
	if(kernel_if_c != NULL) {
        // I feel like as we use these methods on every syscall we should remove the indirection
        memcpy(&kernel_if, kernel_if_c, sizeof(kernel_if_t));
    }

    init_nano_if_sys(); // <- this allows us to use non sys versions by calling syscall in advance for each function

	act_self_ctrl = self_ctrl;

    init_kernel_if_t(&kernel_if, self_ctrl);

	act_self_ref  = syscall_act_ctrl_get_ref(self_ctrl);

	act_self_queue = queue;

    sync_state = (sync_state_t){.sync_caller = NULL, .sync_token = NULL};
}

res_t simple_res_alloc(size_t length) {
    if(memmgt_ref == NULL) return NULL;

    assert(own_mop != NULL);

    if(length > MAX_SIMPLE_ALLOC) {
        return mem_request(0, length, NONE, own_mop);
    }

    if(res_pool == NULL || length > pool_remains) {
        res_pool = mem_request(0, MAX_SIMPLE_ALLOC, NONE, own_mop);
        pool_remains = MAX_SIMPLE_ALLOC;
    }

    res_t result = res_pool;

    if(pool_remains > length + (2 * RES_META_SIZE)) {
        res_pool = rescap_split(res_pool, length);
        pool_remains -= (length + RES_META_SIZE);
    } else {
        res_pool = NULL;
    }

    return result;
}

void ctor_null(void) {
	return;
}

void dtor_null(void) {
	return;
}