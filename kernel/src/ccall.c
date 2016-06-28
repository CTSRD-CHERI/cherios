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

#include "klib.h"
#include "kernel.h"
#include "string.h"

/*
 * CCall/CReturn handler
 */

/* Creates a token for synchronous CCalls. This ensures the anwser is unique. */
static void * get_sync_token(aid_t ccaller, u_int sealc3) {
	static uint32_t unique = 0;
	assert(sealc3 <= 1);
	unique++;
	kernel_acts[ccaller].sync_token.expected_reply  = unique;

	uint64_t token_offset = (ccaller << 32) + (sealc3 << 31) + unique;
	void * sync_token = cheri_setbounds(cheri_getdefault(), 0);
	sync_token = cheri_setoffset(sync_token, token_offset);
	return kernel_seal(sync_token, 42000);
}

static void kernel_ccall_core(int sync) {
	/* Unseal CCall cs and cb */
	/* cb is the activation and cs the cookie */
	void * cs = kernel_exception_framep_ptr->cf_c2;
	proc_t * cb = kernel_exception_framep_ptr->cf_c1;
	int otype = cheri_gettype(cb);
	cs = kernel_unseal(cs, otype);
	cb = kernel_unseal(cb, otype);

	void * sync_token = NULL;
	if(sync) {
		int sealc3 = 0;
		if(((long)kernel_exception_framep[kernel_curr_proc].mf_v0) == -1) {
			sealc3 = 1;
		}

		sync_token = get_sync_token(kernel_curr_proc, sealc3);
	}

	/* Push the message on the queue */
	msg_push(cb->aid, kernel_curr_proc, cs, sync_token);

	if(sync) {
		kernel_acts[kernel_curr_proc].status = status_sync_block;
		kernel_reschedule();
	}
}

void kernel_ccall(void) {
	KERNEL_TRACE(__func__, "in");

	uint32_t ccall_selector =
	  *((uint32_t *)kernel_exception_framep[kernel_curr_proc].cf_pcc) & 0x7FF;

	/* Ack ccall instruction */
	kernel_skip_pid(kernel_curr_proc);

	switch(ccall_selector) {
		case 1: /* send */
			kernel_ccall_core(0);
			break;
		case 2: /* send & switch */
			kernel_ccall_core(0);
			kernel_reschedule();
			break;
		case 3: /* send & wait */
			kernel_ccall_core(0);
			act_wait(kernel_curr_proc);
			break;
		case 4: /* sync call */
			kernel_ccall_core(1);
			break;
		default:
			KERNEL_ERROR("unknown ccall selector '%x'", ccall_selector);
			break;
	}
}

void kernel_creturn(void) {
	KERNEL_TRACE(__func__, "in");

	/* Ack creturn instruction */
	kernel_skip_pid(kernel_curr_proc);

	sync_t * sync_token = kernel_exception_framep[kernel_curr_proc].cf_c1;
	if(sync_token == NULL) {
		/* alias to "wait". Used by asynchronous primitives */
		act_wait(kernel_curr_proc);
		return;
	}

	/* Check if we expect this anwser */
	sync_token = kernel_unseal(sync_token, 42000);
	size_t sync_offset = cheri_getoffset(sync_token);
	aid_t ccaller = sync_offset >> 32;
	uint64_t unique = sync_offset & 0xFFFFFFF;
	if(kernel_acts[ccaller].sync_token.expected_reply != unique ) {
		KERNEL_ERROR("bad sync creturn");
		kernel_freeze();
	}

	/* Make the caller runnable again */
	assert(kernel_acts[ccaller].status = status_sync_block);
	kernel_acts[ccaller].status = status_runnable;

	/* Seal the identifier if needed */
	int sealc3 = (sync_offset >> 31) & 1;
	void * toseal = kernel_exception_framep[kernel_curr_proc].cf_c3;
	if(sealc3) {
		kernel_exception_framep[kernel_curr_proc].cf_c3 =
		  kernel_seal(cheri_andperm(toseal, 0b111100011111101), kernel_curr_proc);
	}

	/* Copy return values */
	kernel_exception_framep[ccaller].cf_c3 =
	   kernel_exception_framep[kernel_curr_proc].cf_c3;
	kernel_exception_framep[ccaller].mf_v0 =
	   kernel_exception_framep[kernel_curr_proc].mf_v0;
	kernel_exception_framep[ccaller].mf_v1 =
	   kernel_exception_framep[kernel_curr_proc].mf_v1;

	/* Try to set the callee in waiting mode */
	act_wait(kernel_curr_proc);
}
