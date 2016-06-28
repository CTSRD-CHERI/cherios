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
#include "kernel.h"
#include "string.h"

/*
 * Routines to handle activations
 */

/* We only create activations for now, no delete */
struct reg_frame		kernel_exception_framep[MAX_ACTIVATIONS];
struct reg_frame *		kernel_exception_framep_ptr;
proc_t				kernel_acts[MAX_ACTIVATIONS];
aid_t 				kernel_curr_proc;
static aid_t			kernel_next_act;

static int			act_default_id_0;
static const void *		act_default_id = &act_default_id_0;

void act_init(void) {
	KERNEL_TRACE("boot", "activation init");
	kernel_curr_proc = 0;
	kernel_next_act = 1;
	kernel_acts[kernel_curr_proc].status = status_runnable;
	kernel_exception_framep_ptr = &kernel_exception_framep[kernel_curr_proc];
}

static void act_schedule(aid_t act) {
	assert(kernel_acts[act].status == status_schedulable);
	/* Set message */
	msg_pop(act);

	/* Activation ready to be run */
	kernel_acts[act].status = status_runnable;
}

void kernel_reschedule(void) {
	kernel_assert(kernel_curr_proc < kernel_next_act);
	#ifdef __TRACE__
	size_t old_kernel_curr_proc = kernel_curr_proc;
	#endif
	int tries = 0;
	again:
	if(++kernel_curr_proc == kernel_next_act) {
		kernel_curr_proc = 0;
	}
	tries++;
	if(tries > MAX_ACTIVATIONS) {
		KERNEL_ERROR("No activation to schedule");
		kernel_freeze();
	}
	if(kernel_acts[kernel_curr_proc].status == status_schedulable) {
		act_schedule(kernel_curr_proc);
	}
	if(kernel_acts[kernel_curr_proc].status != status_runnable) {
		goto again;
	}
	kernel_exception_framep_ptr = kernel_exception_framep + kernel_curr_proc;
	KERNEL_TRACE("exception", "Reschedule from task '%ld' to task '%ld'",
		old_kernel_curr_proc, kernel_curr_proc );
}

void kernel_skip_pid(int pid) {
	kernel_exception_framep[pid].mf_pc += 4; //assuming no branch delay slot
	void * pcc = (void *) kernel_exception_framep[pid].cf_pcc;
	pcc = __builtin_memcap_offset_increment(pcc, 4);
	kernel_exception_framep[pid].cf_pcc = pcc;
}

void kernel_skip(void) {
	kernel_exception_framep_ptr->mf_pc += 4; //assuming no branch delay slot
	void * pcc = (void *) kernel_exception_framep_ptr->cf_pcc;
	pcc = __builtin_memcap_offset_increment(pcc, 4);
	kernel_exception_framep_ptr->cf_pcc = pcc;
}

static void * act_create_ref(aid_t aid) {
	return kernel_seal(kernel_cap_to_exec(CHERI_ELEM(kernel_acts, aid)), aid);
}

static void * act_create_ctrl_ref(aid_t aid) {
	return kernel_seal(CHERI_ELEM(kernel_acts, aid), 42001);
}

void * act_register(const reg_frame_t * frame) {
	aid_t aid = kernel_next_act;

	/* set aid */
	kernel_acts[aid].aid = aid;

	/* set parent */
	kernel_acts[aid].parent = kernel_curr_proc;

	/* set register frame */
	memcpy(kernel_exception_framep + aid, frame, sizeof(struct reg_frame));

	/* set queue */
	kernel_acts[aid].queue.start = 0;
	kernel_acts[aid].queue.end = 0;
	kernel_acts[aid].queue.len = MAX_MSG;
	kernel_acts[aid].queue_len = MAX_MSG;

	/* set reference */
	kernel_acts[aid].act_reference = act_create_ref(aid);

	/* set default identifier */
	kernel_acts[aid].act_default_id = kernel_seal(act_default_id, aid);

	/* set status */
	kernel_acts[aid].status = status_waiting;

	KERNEL_TRACE("exception", "%s OK! ", __func__);
	/* done, update next_act */
	kernel_next_act++;
	return act_create_ctrl_ref(aid);
}

void * act_get_ref(proc_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	aid_t aid = ctrl->aid;
	return kernel_acts[aid].act_reference;
}

void * act_get_id(proc_t * ctrl) {
	ctrl = kernel_unseal(ctrl, 42001);
	aid_t aid = ctrl->aid;
	return kernel_acts[aid].act_default_id;
}

void act_wait(int act) {
	assert(kernel_acts[act].status == status_runnable);
	kernel_acts[act].status = msg_try_wait(act);
	kernel_reschedule();
}
