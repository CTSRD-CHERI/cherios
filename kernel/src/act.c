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

#include "sys/types.h"
#include "activations.h"
#include "klib.h"
#include "queue.h"
#include "namespace.h"
#include "msg.h"
#include "ccall_trampoline.h"
#include "nanokernel.h"

/*
 * Routines to handle activations
 */

act_t				kernel_acts[MAX_ACTIVATIONS]  __sealable;
aid_t				kernel_next_act;

/* Really belongs to the sched, we will eventually seperate out the activation manager */
act_t * 			kernel_curr_act;

// TODO: Put these somewhere sensible;
static queue_default_t boot_queue, kernel_queue;
static kernel_if_t internel_if;
static act_t* ns_ref = NULL;

static kernel_if_t* get_if() {
	return (kernel_if_t*) cheri_andperm(&internel_if, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
}

static act_t * act_create_sealed_ref(act_t * act) {
	return (act_t *)kernel_seal(act, act_ref_type);
}

static act_control_t * act_create_sealed_ctrl_ref(act_t * act) {
	return (act_control_t *)kernel_seal(act, act_ctrl_ref_type);
}

void act_init(context_t boot_context, context_t own_context, struct boot_hack_t* hack) {
	KERNEL_TRACE("init", "activation init");


	internel_if.message_send = kernel_seal(act_send_message_get_trampoline(), act_ref_type);
	internel_if.message_reply = kernel_seal(act_send_return_get_trampoline(), act_sync_ref_type);
	setup_syscall_interface(&internel_if);

	kernel_next_act = 0;

	// This is a dummy. We have already created these contexts
	reg_frame_t frame;
	bzero(&frame, sizeof(struct reg_frame));

	// Register these first two contexts
	act_register(&frame, &kernel_queue.queue, "kernel", 0, status_terminated, NULL, 0);
	bzero(&frame, sizeof(struct reg_frame));
	act_register(&frame, &boot_queue.queue, "boot", 0, status_alive, NULL, 0);

	act_t * kernel_act = &kernel_acts[0];
	act_t * boot_act = &kernel_acts[namespace_num_boot];

	/* The contexts already exist and we set them here */
	kernel_act->context = own_context;
	boot_act->context = boot_context;

	/* While boot is still a user program, we need some way to pass these values. It was created too early for them to
	 * exist */
	hack->kernel_if_c = get_if();
	hack->queue = boot_act->msg_queue;
	hack->self_ctrl = (act_control_kt)act_create_sealed_ctrl_ref(boot_act);

	/* The boot activation should be the current activation */
	sched_schedule(boot_act);
}

act_t * act_register(reg_frame_t *frame, queue_t *queue, const char *name, register_t a0,
							status_e create_in_status, act_control_t *parent, size_t base) {
	(void)parent;
	KERNEL_TRACE("act", "Registering activation %s", name);
	if(kernel_next_act >= MAX_ACTIVATIONS) {
		kernel_panic("no act slot");
	}

	act_t * act = kernel_acts + kernel_next_act;

	act->image_base = base;

	//TODO bit of a hack. the kernel needs to know what namespace service to use
	if(kernel_next_act == namespace_num_namespace) {
		KERNEL_TRACE("act", "found namespace");
		ns_ref = act_create_sealed_ref(act);
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
	/* Setup frame to have its own ctrl and a0 */
	//FIXME this convention needs work, it was copied from what was expected by init.S in libuser.
	//FIXME we might as well have every reference needed, and not then have the user get them from the ctrl.
	//FIXME I also feel that getting the namespace should be a syscall?
	/* set namespace */
	frame->cf_c23	= (capability)ns_ref;
	frame->cf_c24	= (capability)get_if();
	frame->cf_c25	= (capability)queue;
	frame->mf_a0 = a0;
	frame->cf_c5 = (capability)act_create_sealed_ctrl_ref(act);

	/* set queue */
	msg_queue_init(act, queue);

	/* set expected sequence to not expecting */
	act->sync_state.sync_token = 0;
	act->sync_state.sync_condition = 0;

	/* set scheduling status */
	sched_create(act);

	/*update next_act */
	kernel_next_act++;
	KERNEL_TRACE("register", "image base of %s is %lx", act->name, act->image_base);
	KERNEL_TRACE("act", "%s OK! ", __func__);
	return act;
}

act_control_t *act_register_create(reg_frame_t *frame, queue_t *queue, const char *name, register_t a0,
							status_e create_in_status, act_control_t *parent) {
	act_t* act = act_register(frame, queue, name, a0, create_in_status, parent, (size_t)cheri_getbase(frame->cf_pcc));
	act->context = create_context(frame, 0);
	return act_create_sealed_ctrl_ref(act);
}

int act_revoke(act_control_t * ctrl) {
	if(ctrl->status == status_terminated) {
		return -1;
	}
	ctrl->status = status_revoked;
	return 0;
}

int act_terminate(act_control_t * ctrl) {
	ctrl->status = status_terminated;
	KERNEL_TRACE("act", "Terminating %s", ctrl->name);
	/* This will never return if this is a self terminate. We will be removed from the queue and descheduled */
	sched_delete(ctrl);
	if(ctrl == kernel_curr_act) { /* terminated itself */
		kernel_panic("Should not reach here");
	}
	return 0;
}

act_t * act_get_sealed_ref_from_ctrl(act_control_t * ctrl) {
	return act_create_sealed_ref(ctrl);
}


status_e act_get_status(act_control_t *ctrl) {
	KERNEL_TRACE("get status", "%s", ctrl->name);
	return ctrl->status;
}