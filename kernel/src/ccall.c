/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "klib.h"
#include "cp0.h"

/*
 * CCall/CReturn handler
 */

/* Creates a token for synchronous CCalls. This ensures the answer is unique. */
static capability get_and_set_sealed_sync_token(act_t* ccaller) {
	// FIXME No static local variables
	static sync_t unique = 0;
	unique ++;
	ccaller->sync_token = unique;

	capability sync_token = cheri_andperm(cheri_getdefault(), 0);
	#ifdef _CHERI256_
	sync_token = cheri_setbounds(sync_token, 0);
	#endif
	sync_token = cheri_setoffset(sync_token, unique);
	return kernel_seal(sync_token, act_sync_type);
}

static sync_t unseal_sync_token(capability token) {
	token = kernel_unseal(token, act_sync_type);
	return cheri_getoffset(token);
}

static int token_expected(act_t* ccaller, capability token) {
	sync_t got = unseal_sync_token(token);
	return ccaller->sync_token == got;
}

static void kernel_ccall_core(int cflags) {
	/* Unseal CCall cs and cb */
	/* cb is the activation and cs the identifier */
	capability object_identifier = kernel_exception_framep_ptr->cf_c2;
	act_t * act = kernel_exception_framep_ptr->cf_c1;

	int otype = cheri_gettype(act);
	object_identifier = kernel_unseal(object_identifier, otype);
	act = kernel_unseal(act, otype);
	/* FIXME: This is needed because we have screwed CCall */
	act = kernel_cap_make_rw(act);

	if(!(cheri_getperm(object_identifier) & CHERI_PERM_STORE)) {
		KERNEL_ERROR("Bad identifier: missing store permission");
		return;
	}

	if(!(cheri_getperm(act) & CHERI_PERM_STORE)) {
		KERNEL_ERROR("Bad activation ref: missing store permission");
		return;
	}

	if(act->status != status_alive) {
		KERNEL_ERROR("Trying to CCall revoked activation %s",
		             act->name);
		return;
	}

	capability sync_token = NULL;
	if(cflags & 2) {
		sync_token = get_and_set_sealed_sync_token(kernel_curr_act);
	}

	/* Push the message on the queue */
	if(msg_push(act, kernel_curr_act, object_identifier, sync_token)) {
		//KERNEL_ERROR("Queue full");
		if(cflags & 2) {
			kernel_panic("queue full (csync)");
		}
		kernel_exception_framep_ptr->mf_v0 = 0;
	}

	if(cflags & 2) {
		sched_a2d(kernel_curr_act, sched_sync_block);
		sched_reschedule(act);
	}
	if(cflags & 1) {
		act_wait(kernel_curr_act, act);
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
			KERNEL_ERROR("unknown ccall selector '%x'", ccall_selector);
			return;
	}
	kernel_ccall_core(cflags);
}

void kernel_creturn(void) {
	KERNEL_TRACE(__func__, "in");

	/* Ack creturn instruction */
	kernel_skip_instr(kernel_curr_act);

	capability sync_token = kernel_exception_framep_ptr->cf_c1;
	if(sync_token == NULL) {
		KERNEL_TRACE(__func__, "%s makes a non synchronous return", kernel_curr_act->name);
		/* Used by asynchronous primitives */
		//act_wait(kernel_curr_act, 0);
		act_wait(kernel_curr_act, kernel_curr_act);
		return;
	}

	/* Check if we expect this anwser */
	act_t * caller = kernel_exception_framep_ptr->cf_c2;
	if (cheri_gettype(caller) != act_sync_ref_type) {
		KERNEL_ERROR("Activation %s tried to make a synchronous return but provided an incorrect caller",
					 kernel_curr_act->name);
		CHERI_PRINT_CAP(caller);
		kernel_freeze();
	}
	caller = kernel_unseal(caller, act_sync_ref_type);

	if(!token_expected(caller, sync_token)) {
		KERNEL_ERROR("bad sync creturn");
		kernel_freeze();
	}

	KERNEL_TRACE(__func__, "%s correctly makes a sync return to %s", kernel_curr_act->name, caller->name);

	/* Must no longer expect this sequence token */
	caller->sync_token = 0;

	/* Make the caller runnable again */
	kernel_assert(caller->sched_status == sched_sync_block);
	sched_d2a(caller, sched_runnable);

	/* Copy return values */
	caller->saved_registers.cf_c3 =
	   kernel_exception_framep_ptr->cf_c3;
	caller->saved_registers.mf_v0 =
	   kernel_exception_framep_ptr->mf_v0;
	caller->saved_registers.mf_v1 =
	   kernel_exception_framep_ptr->mf_v1;

	/* Try to set the callee in waiting mode */
	act_wait(kernel_curr_act, caller);
}
