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
#include "queue.h"

/*
 * Routines to handle activations
 */

/* We only create activations for now, no delete */
struct reg_frame *		kernel_exception_framep_ptr;
act_t				kernel_acts[MAX_ACTIVATIONS]  __sealable;
aid_t				kernel_next_act;
act_t * 			kernel_curr_act;
static capability            act_default_id = NULL;

// TODO: Put these somewhere sensible;
queue_default_t boot_queue, kernel_queue;

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
	act_register(&dummy_frame, &kernel_queue.queue, "kernel");
	kernel_acts[0].status = status_terminated;
	kernel_acts[0].sched_status = sched_terminated;

	/* create the boot activation (activation that called us) */
	aid_t boot_id = 1;
	act_t* boot_act = &kernel_acts[boot_id];
	kernel_curr_act = boot_act;

	kernel_exception_framep_ptr = &boot_act->saved_registers;
	act_register(&boot_act->saved_registers, &boot_queue.queue, "boot");
	sched_d2a(boot_act, sched_runnable);
}

void kernel_skip_instr(act_t* act) {
	act->saved_registers.mf_pc += 4; /* assumes no branch delay slot */
	void * pcc = (void *) act->saved_registers.cf_pcc;
	pcc = __builtin_memcap_offset_increment(pcc, 4);
	act->saved_registers.cf_pcc = pcc;
}

static act_t * act_create_sealed_ref(act_t * act) {
	return kernel_seal(kernel_cap_make_rx(act), act_ref_type);
}

static act_control_t * act_create_sealed_ctrl_ref(act_t * act) {
	return kernel_seal(act, act_ctrl_ref_type);
}

act_control_t * act_register(const reg_frame_t * frame, queue_t * queue, const char * name) {

	KERNEL_TRACE("act", "Registering activation %s", name);
	if(kernel_next_act >= MAX_ACTIVATIONS) {
		kernel_panic("no act slot");
	}

	act_t * act = kernel_acts + kernel_next_act;

	#ifndef __LITE__
	/* set name */
	kernel_assert(ACT_NAME_MAX_LEN > 0);
	int name_len = 0;
	if(VCAP(name, 1, VCAP_R)) {
		name_len = imin(cheri_getlen(name), ACT_NAME_MAX_LEN-1);
	}
	for(int i = 0; i < name_len; i++) {
		char c = name[i];
		act->name[i] = c; /* todo: sanitize the name if we do not trust it */
	}
	act->name[name_len] = '\0';
	#endif

	/* set status */
	act->status = status_alive;

	/* set register frame */
	memcpy(&(act->saved_registers), frame, sizeof(struct reg_frame));

	/* set queue */
	msg_queue_init(act, queue);

	/* set default identifier */
	act->act_default_id = kernel_seal(act_default_id, act_id_type);

	/* set scheduling status */
	sched_create(act);

	/* set expected sequence to not expecting */
	act->sync_token = 0;

	KERNEL_TRACE("act", "%s OK! ", __func__);
	/* done, update next_act */
	kernel_next_act++;
	return act_create_sealed_ctrl_ref(act);
}

int act_revoke(act_control_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	if(ctrl->status == status_terminated) {
		return -1;
	}
	ctrl->status = status_revoked;
	return 0;
}

int act_terminate(act_control_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	ctrl->status = status_terminated;
	sched_delete(ctrl);
	ctrl->sched_status = sched_terminated;
	KERNEL_TRACE("act", "Terminated %s", ctrl->name);
	if(ctrl == kernel_curr_act) { /* terminated itself */
		return 1;
	}
	return 0;
}

act_t * act_get_sealed_ref_from_ctrl(act_control_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	return act_create_sealed_ref(ctrl);
}

capability act_get_id(act_control_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	return ctrl->act_default_id;
}

int act_get_status(act_control_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	return ctrl->status;
}

void act_wait(act_t* act, act_t* next_hint) {
	kernel_assert(act->sched_status == sched_runnable);
	if(msg_queue_empty(act)) {
		sched_a2d(act, sched_waiting);
	} else {
		act->sched_status = sched_schedulable;
	}
	sched_reschedule(next_hint);
}

/* FIXME have to think about these as well */
capability act_seal_identifier(capability identifier) {
	return kernel_seal(cheri_andperm(identifier, 0b111100011111101), act_id_type);
}
