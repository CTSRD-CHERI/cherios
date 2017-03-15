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
#include "cp0.h"

/*
 * CCall/CReturn handler
 */

/* Creates a token for synchronous CCalls. This ensures the answer is unique. */
static void * get_sync_token(aid_t ccaller) {
	static uint32_t unique = 0;
	unique++;
	kernel_acts[ccaller].sync_token.expected_reply  = unique;

	uint64_t token_offset = (((u64)ccaller) << 32) + unique;
	void * sync_token = cheri_andperm(cheri_getdefault(), 0);
	#ifdef _CHERI256_
	sync_token = cheri_setbounds(sync_token, 0);
	#endif
	sync_token = cheri_setoffset(sync_token, token_offset);
	return kernel_seal(sync_token, 42000);
}

static void kernel_ccall_core(int cflags) {
	/* Unseal CCall cs and cb */
	/* cb is the activation and cs the identifier */
	void * cs = kernel_exception_framep_ptr->cf_c2;
	act_t * cb = kernel_exception_framep_ptr->cf_c1;
	int otype = cheri_gettype(cb);
	cs = kernel_unseal(cs, otype);
	cb = kernel_unseal(cb, otype);

	if(!(cheri_getperm(cs) & CHERI_PERM_STORE)) {
		KERNEL_ERROR("Bad identifier: missing store permission");
		return;
	}

	if(cb->status != status_alive) {
		KERNEL_ERROR("Trying to CCall revoked activation %s-%d",
		             cb->name, cb->aid);
		return;
	}

	void * sync_token = NULL;
	if(cflags & 2) {
		sync_token = get_sync_token(kernel_curr_act);
	}

	/* Push the message on the queue */
	if(msg_push(cb->aid, kernel_curr_act, cs, sync_token)) {
		//KERNEL_ERROR("Queue full");
		if(cflags & 2) {
			kernel_panic("queue full (csync)");
		}
		kernel_exception_framep_ptr->mf_v0 = 0;
	}

	if(cflags & 2) {
		sched_a2d(kernel_curr_act, sched_sync_block);
		sched_reschedule(cb->aid);
	}
	if(cflags & 1) {
		act_wait(kernel_curr_act, cb->aid);
	}
}

void kernel_ccall(void) {
	KERNEL_TRACE(__func__, "in");

	register_t ccall_selector =
	#ifdef HARDWARE_fpga
	        cp0_badinstr_get();
	#else
	        *((uint32_t *)kernel_exception_framep_ptr->cf_pcc);
	#endif
	ccall_selector &= 0x7FF;

	/* Ack ccall instruction */
	kernel_skip_instr(kernel_curr_act);

	int cflags;

	switch(ccall_selector) {
		case 1: /* send */
			cflags = 0;
			break;
		case 2: /* send & switch */
			cflags = 1;
			break;
		case 4: /* sync call */
			cflags = 2;
			break;
		default:
			KERNEL_ERROR("unknown ccall selector '%lu'", ccall_selector);
			return;
	}
	kernel_ccall_core(cflags);
}

void kernel_creturn(void) {
	KERNEL_TRACE(__func__, "in");

	/* Ack creturn instruction */
	kernel_skip_instr(kernel_curr_act);

	sync_t * sync_token = kernel_exception_framep_ptr->cf_c1;
	if(sync_token == NULL) {
		/* Used by asynchronous primitives */
		//act_wait(kernel_curr_act, 0);
		act_wait(kernel_curr_act, kernel_curr_act);
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
	kernel_assert(kernel_acts[ccaller].sched_status == sched_sync_block);
	sched_d2a(ccaller, sched_runnable);

	/* Copy return values */
	kernel_exception_framep[ccaller].cf_c3 =
	   kernel_exception_framep_ptr->cf_c3;
	kernel_exception_framep[ccaller].mf_v0 =
	   kernel_exception_framep_ptr->mf_v0;
	kernel_exception_framep[ccaller].mf_v1 =
	   kernel_exception_framep_ptr->mf_v1;

	/* Try to set the callee in waiting mode */
	act_wait(kernel_curr_act, ccaller);
}
