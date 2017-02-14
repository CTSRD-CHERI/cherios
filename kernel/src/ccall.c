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

#include <activations.h>
#include "klib.h"
#include "syscalls.h"
#include "cp0.h"

/*
 * CCall/CReturn handler
 */

DEFINE_ENUM_CASE(ccall_selector_t, CCALL_SELECTOR_LIST)

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
	if(ccaller->sync_token != got) {
		KERNEL_TRACE("creturn", "caller %s expected %ld got %ld", ccaller->name, ccaller->sync_token, got);
	}
	return ccaller->sync_token == got;
}

static void kernel_ccall_core(ccall_selector_t ccall_selector) {
	/* Unseal CCall cs and cb */
	/* cb is the activation and cs the identifier */
	capability object_identifier = kernel_exception_framep_ptr->cf_c2;
	act_t * act = (act_t *)kernel_exception_framep_ptr->cf_c1;

	uint64_t otype = cheri_gettype(act);
	object_identifier = kernel_unseal(object_identifier, otype);
	act = (act_t *) kernel_unseal(act, otype);
	/* FIXME: This is needed because we have screwed CCall */
	act = (act_t *) kernel_cap_make_rw(act);

	KERNEL_TRACE("ccall", "call is from %s to %s", kernel_curr_act->name, act->name);
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
	if(ccall_selector == SYNC_CALL) {
		sync_token = get_and_set_sealed_sync_token(kernel_curr_act);
	}

	/* Push the message on the queue */
	if(msg_push(act, kernel_curr_act, object_identifier, sync_token)) {
		//KERNEL_ERROR("Queue full");
		if(ccall_selector == SYNC_CALL) {
			kernel_panic("queue full (csync)");
		}
		kernel_exception_framep_ptr->mf_v0 = 0;
	}

	if(ccall_selector == SYNC_CALL) {
		sched_block(kernel_curr_act, sched_sync_block, act);
	}
	if(ccall_selector == SEND_SWITCH) {
		act_wait(kernel_curr_act, act);
	}
}

void kernel_ccall(void) {
	KERNEL_TRACE(__func__, "in");

	ccall_selector_t ccall_selector = (ccall_selector_t)
	#ifdef HARDWARE_fpga
	        cp0_badinstr_get();
	#else
	        *((uint32_t *)kernel_exception_framep_ptr->cf_pcc);
	#endif
	ccall_selector &= 0x7FF;

	KERNEL_TRACE(__func__, "Selector = %s", enum_ccall_selector_t_tostring(ccall_selector));
	/* Ack ccall instruction */
	kernel_skip_instr(kernel_curr_act);

	if(ccall_selector != SEND && ccall_selector != SEND_SWITCH && ccall_selector != SYNC_CALL) {
		KERNEL_ERROR("unknown ccall selector '%x'", ccall_selector);
		return;
	}

	kernel_ccall_core(ccall_selector);
}

void kernel_creturn(void) {
	KERNEL_TRACE(__func__, "in");

	/* Ack creturn instruction */
	kernel_skip_instr(kernel_curr_act);

	capability sync_token = kernel_exception_framep_ptr->cf_c1;
	if(sync_token == NULL) {
		KERNEL_TRACE(__func__, "%s did not provide a sync token", kernel_curr_act->name);
		kernel_freeze();
	}

	/* Check if we expect this anwser */
	act_t * caller = (act_t *)kernel_exception_framep_ptr->cf_c2;
	if (cheri_gettype(caller) != act_sync_ref_type) {
		KERNEL_ERROR("Activation %s tried to make a synchronous return but provided an incorrect caller",
					 kernel_curr_act->name);
		CHERI_PRINT_CAP(caller);
		kernel_freeze();
	}
	caller = (act_t*)kernel_unseal(caller, act_sync_ref_type);

	if(!token_expected(caller, sync_token)) {
		KERNEL_ERROR("wrong sequence token from creturn");
		kernel_freeze();
	}

	KERNEL_TRACE(__func__, "%s correctly makes a sync return to %s", kernel_curr_act->name, caller->name);

	/* Must no longer expect this sequence token */
	caller->sync_token = 0;

	/* Make the caller runnable again */
	kernel_assert(caller->sched_status == sched_sync_block);
	sched_recieve_ret(caller);

	/* Copy return values */
	caller->saved_registers.cf_c3 =
	   kernel_exception_framep_ptr->cf_c3;
	caller->saved_registers.mf_v0 =
	   kernel_exception_framep_ptr->mf_v0;
	caller->saved_registers.mf_v1 =
	   kernel_exception_framep_ptr->mf_v1;

	/* TODO: does this make sense? The activation may want to continue even if there are no more messages*/
	act_wait(kernel_curr_act, caller);
}
