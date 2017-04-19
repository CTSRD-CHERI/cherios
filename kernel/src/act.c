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
#include "activations.h"
#include "klib.h"
#include "queue.h"
#include "namespace.h"

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
	CHERI_PRINT_CAP(act_default_id);
	/*
	 * create kernel activation
	 * used to have a 'free' reg frame.
	 * canot be scheduled: aid 0 is invalid
	 */
	kernel_next_act = 0;
	struct reg_frame dummy_frame;
	bzero(&dummy_frame, sizeof(struct reg_frame));
	act_register(&dummy_frame, &kernel_queue.queue, "kernel", 0, status_terminated);

	/* create the boot activation (activation that called us) */
	act_t* boot_act = &kernel_acts[namespace_num_boot];

	act_register(&boot_act->saved_registers, &boot_queue.queue, "boot", 0, status_alive);

	sched_schedule(boot_act);
}

void kernel_skip_instr(act_t* act) {
	act->saved_registers.mf_pc += 4; /* assumes no branch delay slot */
	void * pcc = (void *) act->saved_registers.cf_pcc;
	pcc = __builtin_cheri_offset_increment(pcc, 4);
	act->saved_registers.cf_pcc = pcc;
}

static act_t * act_create_sealed_ref(act_t * act) {
	return kernel_seal(kernel_cap_make_rx(act), act_ref_type);
}

static act_control_t * act_create_sealed_ctrl_ref(act_t * act) {
	return kernel_seal(act, act_ctrl_ref_type);
}

static act_t* ns_ref = NULL;
static capability ns_id  = NULL;

act_control_t * act_register(const reg_frame_t * frame,
							 queue_t * queue, const char * name,
							 register_t a0,
							 status_e create_in_status) {

	KERNEL_TRACE("act", "Registering activation %s", name);
	if(kernel_next_act >= MAX_ACTIVATIONS) {
		kernel_panic("no act slot");
	}


	act_t * act = kernel_acts + kernel_next_act;

	act->image_base = cheri_getbase(frame->cf_c0);

	//TODO bit of a hack. the kernel needs to know what namespace service to use
	if(kernel_next_act == namespace_num_namespace) {
		KERNEL_TRACE("act", "found namespace");
		ns_ref = act_create_sealed_ref(act);
		ns_id = kernel_seal(act_default_id, act_id_type);
	}

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
	act->status = create_in_status;

	/* set register frame */
	memcpy(&(act->saved_registers), frame, sizeof(struct reg_frame));
	/* Setup frame to have its own ctrl and a0 */
	//FIXME this convention needs work, it was copied from what was expected by init.S in libuser.
	//FIXME we might as well have every reference needed, and not then have the user get them from the ctrl.
	//FIXME I also feel that getting the namespace should be a syscall?
	/* set namespace */
	act->saved_registers.cf_c23	= ns_ref;
	act->saved_registers.cf_c24	= ns_id;
	act->saved_registers.cf_c25	= queue;

	act->saved_registers.mf_a0 = a0;
	act->saved_registers.cf_c5 = act_create_sealed_ctrl_ref(act);
	/* set queue */
	msg_queue_init(act, queue);

	/* set default identifier */
	act->act_default_id = kernel_seal(act_default_id, act_id_type);


	/* set expected sequence to not expecting */
	act->sync_token = 0;

	/* set scheduling status */
	sched_create(act);

	/*update next_act */
	kernel_next_act++;
	KERNEL_TRACE("register", "image base of %s is %lx", act->name, act->image_base);
	KERNEL_TRACE("act", "%s OK! ", __func__);
	return act_create_sealed_ctrl_ref(act);
}

int act_revoke(act_control_t * ctrl) {
	ctrl = (act_control_t *) kernel_unseal(ctrl, act_ctrl_ref_type);
	if(ctrl->status == status_terminated) {
		return -1;
	}
	ctrl->status = status_revoked;
	return 0;
}

int act_terminate(act_control_t * ctrl) {
	ctrl = (act_control_t *) kernel_unseal(ctrl, act_ctrl_ref_type);
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
	ctrl = (act_control_t *) kernel_unseal(ctrl, act_ctrl_ref_type);
	return act_create_sealed_ref(ctrl);
}

capability act_get_id(act_control_t * ctrl) {
	ctrl = (act_control_t *) kernel_unseal(ctrl, act_ctrl_ref_type);
	return ctrl->act_default_id;
}

int act_get_status(act_control_t * ctrl) {
	ctrl = (act_control_t *) kernel_unseal(ctrl, act_ctrl_ref_type);
	KERNEL_TRACE("get status", "%s", ctrl->name);
	return ctrl->status;
}

void act_wait(act_t* act, act_t* next_hint) {
	if(msg_queue_empty(act)) {
		sched_block(act, sched_waiting, next_hint);
	} else {
		return;
	}
}

/* FIXME have to think about these as well */
capability act_seal_identifier(capability identifier) {
	return kernel_seal(cheri_andperm(identifier, 0b111100011111101), act_id_type);
}
