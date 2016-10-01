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

/*
 * Routines to handle activations
 */

/* We only create activations for now, no delete */
struct reg_frame		kernel_exception_framep[MAX_ACTIVATIONS];
struct reg_frame *		kernel_exception_framep_ptr;
act_t				kernel_acts[MAX_ACTIVATIONS]  __sealable;
aid_t 				kernel_curr_act;
aid_t				kernel_next_act;

static const void *            act_default_id = NULL;

void act_init(void) {
	KERNEL_TRACE("init", "activation init");

	/* initialize the default identifier to a known value */
	act_default_id = cheri_setbounds(cheri_getdefault(), 0);

	/*
	 * create kernel activation
	 * used to have a 'free' reg frame.
	 * canot be scheduled: aid 0 is invalid
	 */
	kernel_next_act = 0;
	struct reg_frame dummy_frame;
	bzero(&dummy_frame, sizeof(struct reg_frame));
	act_register(&dummy_frame, "kernel");
	kernel_acts[0].status = status_terminated;
	kernel_acts[0].sched_status = sched_terminated;

	/* create the boot activation (activation that called us) */
	kernel_curr_act = 1;
	kernel_exception_framep_ptr = &kernel_exception_framep[kernel_curr_act];
	act_register(kernel_exception_framep, "boot");
	sched_d2a(kernel_curr_act, sched_runnable);
}

void kernel_skip_instr(aid_t act) {
	kernel_exception_framep[act].mf_pc += 4; /* assumes no branch delay slot */
	void * pcc = (void *) kernel_exception_framep[act].cf_pcc;
	pcc = __builtin_memcap_offset_increment(pcc, 4);
	kernel_exception_framep[act].cf_pcc = pcc;
}

static void * act_create_ref(aid_t aid) {
	return kernel_seal(kernel_cap_to_exec(kernel_acts + aid), aid);
}

static void * act_create_ctrl_ref(aid_t aid) {
	return kernel_seal(kernel_acts + aid, 42001);
}

void * act_register(const reg_frame_t * frame, const char * name) {
	aid_t aid = kernel_next_act;

	if(aid >= MAX_ACTIVATIONS) {
		kernel_panic("no act slot");
	}

	/* set aid */
	kernel_acts[aid].aid = aid;

	#ifndef __LITE__
	/* set name */
	kernel_assert(ACT_NAME_MAX_LEN > 0);
	int name_len = 0;
	if(VCAP(name, 1, VCAP_R)) {
		name_len = imin(cheri_getlen(name), ACT_NAME_MAX_LEN-1);
	}
	for(int i = 0; i < name_len; i++) {
		char c = name[i];
		kernel_acts[aid].name[i] = c; /* todo: sanitize the name if we do not trust it */
	}
	kernel_acts[aid].name[name_len] = '\0';
	#endif

	/* set status */
	kernel_acts[aid].status = status_alive;

	/* set register frame */
	memcpy(kernel_exception_framep + aid, frame, sizeof(struct reg_frame));

	/* set queue */
	msg_queue_init(aid);
	kernel_acts[aid].queue_mask = MAX_MSG-1;

	/* set reference */
	kernel_acts[aid].act_reference = act_create_ref(aid);

	/* set default identifier */
	kernel_acts[aid].act_default_id = kernel_seal(act_default_id, aid);

	/* set scheduling status */
	sched_create(aid);

	KERNEL_TRACE("act", "%s OK! ", __func__);
	/* done, update next_act */
	kernel_next_act++;
	return act_create_ctrl_ref(aid);
}

int act_revoke(act_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	aid_t aid = ctrl->aid;
	if(kernel_acts[aid].status == status_terminated) {
		return -1;
	}
	kernel_acts[aid].status = status_revoked;
	return 0;
}

int act_terminate(act_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	aid_t act = ctrl->aid;
	kernel_acts[act].status = status_terminated;
	sched_delete(act);
	kernel_acts[act].sched_status = sched_terminated;
	KERNEL_TRACE("act", "Terminated %s:%d", kernel_acts[act].name, act);
	if(act == kernel_curr_act) { /* terminated itself */
		return 1;
	}
	return 0;
}

void * act_get_ref(act_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	aid_t aid = ctrl->aid;
	return kernel_acts[aid].act_reference;
}

void * act_get_id(act_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	aid_t aid = ctrl->aid;
	return kernel_acts[aid].act_default_id;
}

int act_get_status(act_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	aid_t aid = ctrl->aid;
	return kernel_acts[aid].status;
}

void act_wait(int act, aid_t next_hint) {
	kernel_assert(kernel_acts[act].sched_status == sched_runnable);
	if(msg_queue_empty(act)) {
		sched_a2d(act, sched_waiting);
	} else {
		kernel_acts[act].sched_status = sched_schedulable;
	}
	sched_reschedule(next_hint);
}

void * act_seal_identifier(void * identifier) {
	return kernel_seal(cheri_andperm(identifier, 0b111100011111101), kernel_curr_act);
}
