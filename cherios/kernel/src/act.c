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
#include <dylink.h>
#include "sys/types.h"
#include "activations.h"
#include "klib.h"
#include "namespace.h"
#include "msg.h"
#include "nano/nanokernel.h"
#include "queue.h"
#include "nano/nanokernel.h"
#include "act_events.h"
#include "atomic.h"

/*
 * Routines to handle activations
 */

act_t				kernel_acts[MAX_STATIC_ACTIVATIONS]  __sealable;
aid_t				kernel_next_act;

/* Really belongs to the sched, we will eventually seperate out the activation manager */
act_t * 			kernel_curr_act;

// TODO: Put these somewhere sensible;
static queue_default_t init_queue, kernel_queue;
static kernel_if_t internel_if;
static act_t* ns_ref = NULL;

struct spinlock_t 	act_list_lock;
act_t* act_list_start;
act_t* act_list_end;

act_t* memgt_ref = NULL;
act_t* event_ref = NULL;

static kernel_if_t* get_if() {
	return (kernel_if_t*) cheri_andperm(&internel_if, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
}

sealing_cap ref_sealer;
sealing_cap ctrl_ref_sealer;
sealing_cap sync_ref_sealer;
sealing_cap sync_token_sealer;
sealing_cap notify_ref_sealer;

capability act_seal_for_call(act_t * act, sealing_cap sealer) {
	return cheri_seal(act, sealer);
}

act_t* act_unseal_callable(act_t * act, sealing_cap sealer) {
	return (act_t*)cheri_unseal(act, sealer);
}

act_t * act_create_sealed_ref(act_t * act) {
	return (act_t *)act_seal_for_call(act, ref_sealer);
}

act_control_t * act_create_sealed_ctrl_ref(act_t * act) {
	return (act_control_t *)act_seal_for_call(act, ctrl_ref_sealer);
}

act_t * act_unseal_ref(act_t * act) {
	return  (act_t *)act_unseal_callable(act, ref_sealer);
}

act_control_t* act_unseal_ctrl_ref(act_t* act) {
	return (act_control_t*)act_unseal_callable(act, ctrl_ref_sealer);
}

act_t * act_create_sealed_sync_ref(act_t * act) {
	return (act_t *)act_seal_for_call(act, sync_ref_sealer);
}

act_t * act_unseal_sync_ref(act_t * act) {
	return  (act_t *)act_unseal_callable(act, sync_ref_sealer);
}


void act_set_event_ref(act_t* act) {
	event_ref = act;
}

context_t act_init(context_t own_context, init_info_t* info, size_t init_base, size_t init_entry, size_t init_tls_base,
                    capability global_pcc) {
	KERNEL_TRACE("init", "activation init");

    ref_sealer = get_sealing_cap_from_nano(act_ref_type);
    ctrl_ref_sealer = get_sealing_cap_from_nano(act_ctrl_ref_type);
    sync_ref_sealer = get_sealing_cap_from_nano(act_sync_ref_type);
    sync_token_sealer = get_sealing_cap_from_nano(act_sync_type);
	notify_ref_sealer = get_sealing_cap_from_nano(act_notify_ref_type);

	setup_syscall_interface(&internel_if);

	kernel_next_act = 0;

	// This is a dummy. Our first context has already been created
	reg_frame_t frame;
	bzero(&frame, sizeof(struct reg_frame));

	// Register the kernel (exception) activation
	act_t * kernel_act = &kernel_acts[0];
	act_register(&frame, &kernel_queue.queue, "kernel", status_terminated, NULL, cheri_getbase(global_pcc), NULL);
	/* The kernel context already exists and we set it here */
	kernel_act->context = own_context;
    sched_create(0, kernel_act);
	// Create and register the init activation
	KERNEL_TRACE("act", "Retroactively creating init activation");

	/* Not a dummy here. We will subset our own c0/pcc for init. init is loaded directly after the kernel */
	bzero(&frame, sizeof(struct reg_frame));
	size_t length = cheri_getlen(cheri_getdefault()) - init_base;

    frame.cf_c0 = cheri_setbounds(cheri_setoffset(cheri_getdefault(), init_base), length);
    capability pcc =  cheri_setbounds(cheri_setoffset(global_pcc, init_base), length);

	frame.cf_c12 = frame.cf_pcc = cheri_setoffset(pcc, init_entry);

	/* provide config info to init.  c3 is the conventional register */
	frame.cf_c3 = info;

    /* init has put its thread locals somewhere sensible (base + 0x100) */
    frame.mf_user_loc = 0x7000 + init_tls_base;
    frame.mf_t0 = cheri_getbase(frame.cf_pcc); // Hacky way to indicate a program base
	act_t * init_act = &kernel_acts[namespace_num_init];
	act_register_create(&frame, &init_queue.queue, "init", status_alive, NULL, NULL, 0);

	/* The boot activation should be the current activation */
	sched_schedule(0, init_act);

	return init_act->context;
}

// FIXME: Locking is a bit heavy here. Better to use a transaction so we can keep pre-emption

static void add_act_to_end_of_list(act_t* act) {
	CRITICAL_LOCKED_BEGIN(&act_list_lock);

	act->list_prev = act_list_end;

	if(act_list_end == NULL) {
		act_list_start = act;
	} else {
		act_list_end->list_next = act;
	}

	act_list_end = act;
	act->list_next = NULL;

	CRITICAL_LOCKED_END(&act_list_lock);
}

static void remove_from_list(act_t* act) {
	CRITICAL_LOCKED_BEGIN(&act_list_lock);

	act_t* prev = act->list_prev;
	act_t* next = act->list_next;

	if(prev == NULL) {
		act_list_start = next;
	} else {
		prev->list_next = next;
	}

	if(next == NULL) {
		act_list_end = prev;
	} else {
		next->list_prev = prev;
	}

	CRITICAL_LOCKED_END(&act_list_lock);
}

static act_t* alloc_static_act(aid_t* aid_used) {

	if(kernel_next_act >= MAX_STATIC_ACTIVATIONS) {
		kernel_panic("no act slot");
	}

	aid_t id;
	ATOMIC_ADD(&kernel_next_act, 32, 1, id)

	act_t* act = kernel_acts + id;
	if(aid_used) *aid_used = id;

    act = cheri_setbounds(act, sizeof(act_t));
	return act;
}

act_t * act_register(reg_frame_t *frame, queue_t *queue, const char *name,
							status_e create_in_status, act_control_t *parent, size_t base, res_t res) {
	(void)parent;
	KERNEL_TRACE("act", "Registering activation %s", name);

	act_t * act = NULL;
    cap_pair pr;

	try_take_res(res, sizeof(act_t), &pr);

    act = (act_t*)pr.data;

	aid_t used = 0;

	if(act == NULL) {
		act = alloc_static_act(&used);
	}

	add_act_to_end_of_list(act);

	/* Push C0 to the bottom of the stack so it can be popped when we ccall in */
	// TODO fill in new ABI stuff

    act->ctl.guard.guard = callable_ready;
    act->ctl.csp = cheri_setoffset(cheri_setbounds(&act->user_kernel_stack, USER_KERNEL_STACK_SIZE), USER_KERNEL_STACK_SIZE);
    // act->ctl.cusp; TODO only if we ever need unsafe stack space in the kernel, which we REALLY should not
    act->ctl.cdl = &entry_stub;
    act->ctl.cds = ctrl_ref_sealer;
    act->ctl.cgp = get_cgp();

	act->image_base = base;

	//TODO bit of a hack. the kernel needs to know what namespace service to use
	if(used == namespace_num_namespace) {
		KERNEL_TRACE("act", "found namespace");
		ns_ref = act_create_sealed_ref(act);
	}
	if(used == namespace_num_memmgt) {
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

	/* SEE libuser/src/init.S for the conventional start up */

	/* set namespace */
	frame->cf_c20	= (capability)queue;
	frame->cf_c21 	= (capability)act_create_sealed_ctrl_ref(act);
	frame->cf_c23	= (capability)ns_ref;
	frame->cf_c24	= (capability)get_if();

	/* set queue */
	msg_queue_init(act, queue);

	/* set expected sequence to not expecting */
	act->sync_state.sync_token = 0;
	act->sync_state.sync_condition = 0;

	KERNEL_TRACE("register", "image base of %s is %lx", act->name, act->image_base);
	KERNEL_TRACE("act", "%s OK! ", __func__);
	return act;
}

act_control_t *act_register_create(reg_frame_t *frame, queue_t *queue, const char *name,
							status_e create_in_status, act_control_t *parent, res_t res, uint8_t cpu_hint) {
	// FIXME pcc base will not be the correct base for secure loaded programs as they start via a trampoline
	// FIXME Only the caller really knows what base to use.
	// FIXME The reason this is going wrong is that we really should be sending an address to proc_man to query
	// FIXME what image it is in. This is hard to do when the system is dying however.
	act_t* act = act_register(frame, queue, name, create_in_status, parent, frame->mf_t0, res);
	act->context = create_context(frame);
    /* set scheduling status */

    if(cpu_hint >= SMP_CORES) cpu_hint = SMP_CORES-1;
    sched_create(cpu_hint, act);
	return act_create_sealed_ctrl_ref(act);
}

int act_revoke(act_control_t * ctrl) {
	if(ctrl->status == status_terminated) {
		return -1;
	}
	ctrl->status = status_revoked;

	if(event_ref != NULL)
		msg_push(act_create_sealed_ref(ctrl), NULL, NULL, NULL, 0, 0 , 0, 0, notify_revoke_port, event_ref, ctrl, NULL);

	return 0;
}

int act_terminate(act_control_t * ctrl) {
	ctrl->status = status_terminated;
	KERNEL_TRACE("act", "Terminating %s", ctrl->name);

	if(event_ref != NULL)
		msg_push(act_create_sealed_ref(ctrl), NULL, NULL, NULL, 0, 0 , 0, 0, notify_terminate_port, event_ref, ctrl, NULL);

	// need to delete from linked list
	remove_from_list(ctrl);

	/* This will never return if this is a self terminate. We will be removed from the queue and descheduled */
	sched_delete(ctrl);

	/* If we get here, we terminated another activation and we are in charge of cleanup. If we delete ourselves
	 * sched will take care of cleanup */
	destroy_context(ctrl->context, NULL);

	return 0;
}

act_t * act_get_sealed_ref_from_ctrl(act_control_t * ctrl) {
	return act_create_sealed_ref(ctrl);
}


status_e act_get_status(act_control_t *ctrl) {
	KERNEL_TRACE("get status", "%s", ctrl->name);
	return ctrl->status;
}
