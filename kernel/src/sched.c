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

/* turn 'schedulable' activation 'act' in 'runnable' state */
static void sched_schedule(act_t* act) {
	kernel_assert(act->sched_status == sched_schedulable);
	/* Set message */
	msg_pop(act);

	/* Activation ready to be run */
	act->sched_status = sched_runnable;
}

/* todo: sleep cpu */
static void sched_nothing_to_run(void) {
	KERNEL_ERROR("No activation to schedule");
	kernel_freeze();
}

static act_t * act_queue[MAX_ACTIVATIONS];
static size_t   act_queue_current = 0;
static size_t   act_queue_end = 0;

static void add_act_to_queue(act_t * act) {
	kernel_assert(act_queue_end != MAX_ACTIVATIONS);
	act_queue[act_queue_end++] = act;
}

static void delete_act_from_queue(act_t * act) {
	size_t index = 0;
	if (act_queue[act_queue_current] == act) {
		index = act_queue_current;
	} else {
		size_t i;
		for(i = 0; i < act_queue_end; i++) {
			if(act_queue[i] == act) {
				index = i;
				break;
			}
		}
		kernel_assert(i != act_queue_end);
	}
	act_queue_end--;
	act_queue[index] = act_queue[act_queue_end];
}

void sched_create(act_t * act) {
	KERNEL_TRACE("sched", "create %s", act->name);
	act->sched_status = sched_waiting;
}

void sched_delete(act_t * act) {
	KERNEL_TRACE("sched", "delete %s", act->name);
	if(act->sched_status != sched_waiting) {
		delete_act_from_queue(act);
	}
	act->status = status_terminated;
}

void sched_d2a(act_t * act, sched_status_e status) {
	KERNEL_TRACE("sched", "add %s", act->name);
	add_act_to_queue(act);
	kernel_assert((status == sched_runnable) || (status == sched_schedulable));
	act->sched_status = status;
}

void sched_a2d(act_t * act, sched_status_e status) {
	KERNEL_TRACE("sched", "rem %s", act->name);
	delete_act_from_queue(act);
	kernel_assert((status == sched_sync_block) || (status == sched_waiting));
	act->sched_status = status;
}

static act_t * sched_picknext(void) {
	if(act_queue_end == 0) {
		return NULL;
	}
	act_queue_current = (act_queue_current + 1) % act_queue_end;
	return act_queue[act_queue_current];
}

/* FIXME do not pop on wakeup */
void sched_reschedule(act_t * hint) {
	#ifdef __TRACE__
	act_t * old_kernel_curr_act = kernel_curr_act;
	#endif
	if(!hint) {
		again:
		hint = sched_picknext();
	}
	if(!hint) {
		sched_nothing_to_run();
	}
	if(hint->sched_status == sched_schedulable) {
		sched_schedule(hint);
	}
	if(hint->sched_status != sched_runnable) {
		goto again;
	}

	kernel_curr_act = hint;
	kernel_exception_framep_ptr = &hint->saved_registers;
	KERNEL_TRACE("sched", "Reschedule from activation '%s' to activation '%s'",
	        old_kernel_curr_act->name, kernel_curr_act->name);
}
