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
#include "sys/types.h"
#include "activations.h"
#include "klib.h"
#include "queue.h"
#include "namespace.h"
#include "msg.h"
#include "ccall_trampoline.h"

/*
 * Routines to handle activations
 */

/* We only create activations for now, no delete */
struct reg_frame *		kernel_exception_framep_ptr;
/* This save frame was used to save boots context during bootload. We move it ASAP and use kernel_exception_framep_ptr */
extern struct reg_frame kernel_init_save_frame;

act_t				kernel_acts[MAX_ACTIVATIONS]  __sealable;
aid_t				kernel_next_act;

/* Really belongs to the sched, we will eventually seperate out the activation manager */
act_t * 			kernel_curr_act;

// TODO: Put these somewhere sensible;
static queue_default_t boot_queue, kernel_queue;
static kernel_if_t internel_if;
static act_t* ns_ref = NULL;

kernel_if_t* get_if() {
	return (kernel_if_t*) cheri_andperm(&internel_if, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
}

void act_init(void) {
	KERNEL_TRACE("init", "activation init");
	/*
	 * create kernel activation
	 * used to have a 'free' reg frame.
	 * aid no longer exists, we simply don't add it to sched by having a creation state of terminated
	 */

	//TODO does not belong here
	/* The message enqueue code capability */
	kernel_setup_trampoline();

	internel_if.message_send = kernel_seal(act_send_message_get_trampoline(), act_ref_type);
	internel_if.message_reply = kernel_seal(act_send_return_get_trampoline(), act_sync_ref_type);
	setup_syscall_interface(&internel_if);

	kernel_next_act = 0;

	act_t * kernel_act = &kernel_acts[0];

	/* We are currently inside the kernel act and it is never restored. So we can just zero its saved context */
	bzero(&kernel_act->saved_registers, sizeof(struct reg_frame));
	act_register(&kernel_act->saved_registers, &kernel_queue.queue, "kernel", 0, status_terminated, NULL);

	/* create the boot activation. This is NOT the activation that called this function.*/
	act_t* boot_act = &kernel_acts[namespace_num_boot];

	/* As boot was created before the kernel, it does not have the normal interface capabilities. As currently it
	 * users libuser it will need a few capabilities to bootstrap object init */
	boot_act->saved_registers.cf_c4 =
			(capability)act_register(&kernel_init_save_frame, &boot_queue.queue, "boot", 0, status_alive, NULL);
	boot_act->saved_registers.cf_c3 = (capability)get_if();
	boot_act->saved_registers.cf_c5 = (capability)boot_act->msg_queue;

	//boot_act->image_base = 0xffffffff80000000 + 0x100000;
	sched_schedule(boot_act);
}

void kernel_skip_instr(act_t* act) {
	act->saved_registers.mf_pc += 4; /* assumes no branch delay slot */
	void * pcc = (void *) act->saved_registers.cf_pcc;
	pcc = __builtin_memcap_offset_increment(pcc, 4);
	act->saved_registers.cf_pcc = pcc;
}

static act_t * act_create_sealed_ref(act_t * act) {
	return (act_t *)kernel_seal(act, act_ref_type);
}

static act_control_t * act_create_sealed_ctrl_ref(act_t * act) {
	return (act_control_t *)kernel_seal(act, act_ctrl_ref_type);
}

act_control_t *act_register(const reg_frame_t *frame, queue_t *queue, const char *name, register_t a0,
							status_e create_in_status, act_control_t *parent) {

	KERNEL_TRACE("act", "Registering activation %s", name);
	if(kernel_next_act >= MAX_ACTIVATIONS) {
		kernel_panic("no act slot");
	}


	act_t * act = kernel_acts + kernel_next_act;

	// FIXME this should be pcc base, but I have to check.
	act->image_base = cheri_getbase(frame->cf_c0);

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
	memcpy(&(act->saved_registers), frame, sizeof(struct reg_frame));
	/* Setup frame to have its own ctrl and a0 */
	//FIXME this convention needs work, it was copied from what was expected by init.S in libuser.
	//FIXME we might as well have every reference needed, and not then have the user get them from the ctrl.
	//FIXME I also feel that getting the namespace should be a syscall?
	/* set namespace */
	act->saved_registers.cf_c23	= ns_ref;
	act->saved_registers.cf_c24	= (capability)get_if();
	act->saved_registers.cf_c25	= queue;

	act->saved_registers.mf_a0 = a0;
	act->saved_registers.cf_c5 = act_create_sealed_ctrl_ref(act);
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
	sched_delete(ctrl);
	ctrl->sched_status = sched_terminated;
	KERNEL_TRACE("act", "Terminated %s", ctrl->name);
	if(ctrl == kernel_curr_act) { /* terminated itself */
		return 1;
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

void act_wait(act_t* act, act_t* next_hint) {
	if(msg_queue_empty(act)) {
		sched_block(act, sched_waiting, next_hint, 0);
	} else {
		return;
	}
}