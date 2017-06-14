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

res_t res_pool;

void object_init(act_control_kt self_ctrl, queue_t * queue, kernel_if_t* kernel_if_c) {

    /* This comes in null for threads other than the first - the interface is not thread local */
	if(kernel_if_c != NULL) {
        // I feel like as we use these methods on every syscall we should remove the indirection
        memcpy(&kernel_if, kernel_if_c, sizeof(kernel_if_t));
    }


	act_self_ctrl = self_ctrl;

    init_kernel_if_t(&kernel_if, self_ctrl);

	act_self_ref  = syscall_act_ctrl_get_ref(self_ctrl);

	act_self_queue = queue;

    sync_state = (sync_state_t){.sync_caller = NULL, .sync_token = NULL};
}

res_t get_res_pool(void) {
    if(res_pool == NULL) {
        cap_pair pr;

        if(try_init_memmgt_ref() == NULL) return NULL;

        mmap_new(0, 0x8000, 0, MAP_PRIVATE | MAP_ANONYMOUS | MAP_RESERVED, &pr);
        res_pool = pr.data;

        if(pr.data == NULL) return NULL;
    }

    return res_pool;
}

res_t grab_res_from_pool(size_t length) {
    res_t old = get_res_pool();

    if(old == NULL) return old;

	res_t new = rescap_split_sys(old, length);
	res_pool = new;
	return old;
}

void ctor_null(void) {
	return;
}

void dtor_null(void) {
	return;
}