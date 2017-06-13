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

#include <syscalls.h>
#include "sys/types.h"
#include "activations.h"
#include "klib.h"
#include "namespace.h"
#include "msg.h"
#include "ccall_trampoline.h"
#include "nano/nanokernel.h"
#include "queue.h"

/*
 * Routines to handle activations
 */

act_t				kernel_acts[MAX_ACTIVATIONS]  __sealable;
aid_t				kernel_next_act;

/* Really belongs to the sched, we will eventually seperate out the activation manager */
act_t * 			kernel_curr_act;

// TODO: Put these somewhere sensible;
static queue_default_t init_queue, kernel_queue;
static kernel_if_t internel_if;
static act_t* ns_ref = NULL;
act_t* memgt_ref = NULL;

static kernel_if_t* get_if() {
	return (kernel_if_t*) cheri_andperm(&internel_if, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
}

act_t * act_create_sealed_ref(act_t * act) {
	return (act_t *)kernel_seal(act, act_ref_type);
}

act_control_t * act_create_sealed_ctrl_ref(act_t * act) {
	return (act_control_t *)kernel_seal(act, act_ctrl_ref_type);
}

act_t * act_unseal_ref(act_t * act) {
	return  (act_t *)kernel_unseal(act, act_ref_type);
}

act_control_t* act_unseal_ctrl_ref(act_t* act) {
	return (act_control_t*)kernel_unseal(act, act_ctrl_ref_type);
}

context_t act_init(context_t own_context, init_info_t* info, size_t init_base, size_t init_entry, size_t init_tls_base) {
	KERNEL_TRACE("init", "activation init");

	setup_syscall_interface(&internel_if);

	kernel_next_act = 0;

	// This is a dummy. Our first context has already been created
	reg_frame_t frame;
	bzero(&frame, sizeof(struct reg_frame));

	// Register the kernel (exception) activation
	act_t * kernel_act = &kernel_acts[0];
	act_register(&frame, &kernel_queue.queue, "kernel", status_terminated, NULL, cheri_getbase(cheri_getpcc()));
	/* The kernel context already exists and we set it here */
	kernel_act->context = own_context;

	// Create and register the init activation
	KERNEL_TRACE("act", "Retroactively creating init activation");

	/* Not a dummy here. We will subset our own c0/pcc for init. init is loaded directly after the kernel */
	bzero(&frame, sizeof(struct reg_frame));
	size_t length = cheri_getlen(cheri_getdefault()) - init_base;

    frame.cf_c0 = cheri_setbounds(cheri_setoffset(cheri_getdefault(), init_base), length);
    capability pcc =  cheri_setbounds(cheri_setoffset(cheri_getpcc(), init_base), length);

	frame.cf_c12 = frame.cf_pcc = cheri_setoffset(pcc, init_entry);

	/* provide config info to init.  c3 is the conventional register */
	frame.cf_c3 = info;

    /* init has put its thread locals somewhere sensible (base + 0x100) */
    frame.mf_user_loc = 0x7000 + init_tls_base;

	act_t * init_act = &kernel_acts[namespace_num_init];
	act_register_create(&frame, &init_queue.queue, "init", status_alive, NULL);

	/* The boot activation should be the current activation */
	sched_schedule(init_act);

	return init_act->context;
}

act_t * act_register(reg_frame_t *frame, queue_t *queue, const char *name,
							status_e create_in_status, act_control_t *parent, size_t base) {
	(void)parent;
	KERNEL_TRACE("act", "Registering activation %s", name);
	if(kernel_next_act >= MAX_ACTIVATIONS) {
		kernel_panic("no act slot");
	}

	act_t * act = kernel_acts + kernel_next_act;

	/* Push C0 to the bottom of the stack so it can be popped when we ccall in */
	act->user_kernel_stack[(USER_KERNEL_STACK_SIZE / sizeof(capability)) -1] = cheri_getdefault();

    act->stack_guard = 0;

	act->image_base = base;

	//TODO bit of a hack. the kernel needs to know what namespace service to use
	if(kernel_next_act == namespace_num_namespace) {
		KERNEL_TRACE("act", "found namespace");
		ns_ref = act_create_sealed_ref(act);
	}
	if(kernel_next_act == namespace_num_memmgt) {
		memgt_ref = act;
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

/*Some "documentation" for the interface between the kernel and activation start                                        *
* These fields are setup by the caller of act_register                                                                  *
*                                                                                                                       *
* a0    : user GP argument (goes to main)                                                                               *
* c3    : user Cap argument (goes to main)                                                                              *
*                                                                                                                       *
* These fields are setup by act_register itself. Although the queue is an argument to the function                      *
*                                                                                                                       *
* c21   : self control reference                                                 										*
* c23   : namespace reference (may be null for init and namespace)                                                      *
* c24   : kernel interface table                                                                                        *
* c25   : queue                                                                                                        */

	/* set namespace */
	frame->cf_c21 	= (capability)act_create_sealed_ctrl_ref(act);
	frame->cf_c23	= (capability)ns_ref;
	frame->cf_c24	= (capability)get_if();
	frame->cf_c25	= (capability)queue;

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

act_control_t *act_register_create(reg_frame_t *frame, queue_t *queue, const char *name,
							status_e create_in_status, act_control_t *parent) {
	act_t* act = act_register(frame, queue, name, create_in_status, parent, (size_t)cheri_getbase(frame->cf_pcc));
	act->context = create_context(frame);
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
