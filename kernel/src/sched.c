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

#include "activations.h"
#include "klib.h"
#include "critical.h"

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
	if(act->status == status_alive) {
		KERNEL_TRACE("sched", "add %s", act->name);
		add_act_to_queue(act);
		act->sched_status = sched_runnable;
	} else {
		act->sched_status = sched_terminated;
	}
}

void sched_delete(act_t * act) {
	KERNEL_TRACE("sched", "delete %s", act->name);
	if(act->sched_status == sched_runnable || act->sched_status == sched_running) {
		delete_act_from_queue(act);
	}
	act->status = status_terminated;
	if(act->sched_status == sched_running) {
		sched_reschedule(NULL, sched_terminated, 0);
	} else {
		act->sched_status = sched_terminated;
	}
}

void sched_receives_msg(act_t * act) {
	if(act->sched_status == sched_waiting) {
		KERNEL_TRACE("sched", "now unblocked %s", act->name);
		act->sched_status = sched_runnable;
		add_act_to_queue(act);
	}
}

void sched_recieve_ret(act_t * act) {
	kernel_assert(act->sched_status == sched_sync_block);
	KERNEL_TRACE("sched", "now unblocked %s", act->name);
	add_act_to_queue(act);
	act->sched_status = sched_runnable;
}

void sched_block_until_msg(act_t * act, act_t * next_hint) {
	// would MUCH prefer to use a lighter lock here, or better yet go lockless and just check nothing went wrong at the
	// end
	kernel_critical_section_enter();
	if(msg_queue_empty(act)) {
		// This block will result in a critical section exit
		sched_block(act, sched_waiting, next_hint, 0);
	} else {
		kernel_critical_section_exit();
	}


}

void sched_block(act_t *act, sched_status_e status, act_t* next_hint, int in_kernel) {
	KERNEL_TRACE("sched", "blocking %s", act->name);
	kernel_assert((status == sched_sync_block) || (status == sched_waiting));

	if(act->sched_status == sched_runnable || act->sched_status == sched_running) {
		delete_act_from_queue(act);
	}

	if(act->sched_status == sched_running) {
		sched_reschedule(next_hint, status, in_kernel);
	}
}

static void sched_deschedule(act_t * act, sched_status_e into_state) {
	kernel_assert(act->sched_status == sched_running);
	KERNEL_TRACE("sched", "Reschedule from activation '%s'", act->name);
	act->sched_status = into_state;
}

void sched_schedule(act_t * act) {
	kernel_assert(act->sched_status == sched_runnable);
	act->sched_status = sched_running;
	kernel_curr_act = act;
	kernel_exception_framep_ptr = &act->saved_registers;
	KERNEL_TRACE("sched", "Reschedule to activation '%s'", kernel_curr_act->name);
}

static act_t * sched_picknext(void) {
	if(act_queue_end == 0) {
		return NULL;
	}
	act_queue_current = (act_queue_current + 1) % act_queue_end;
	act_t * next = act_queue[act_queue_current];
	if(!(next->sched_status == sched_runnable || next->sched_status == sched_running)) {
		KERNEL_TRACE("sched", "%s should not be waiting", next->name);
	}
	kernel_assert(next->sched_status == sched_runnable || next->sched_status == sched_running);
	return next;
}

void swap_state(reg_frame_t* from, reg_frame_t* to);

void sched_reschedule(act_t *hint, sched_status_e into_state, int in_kernel) {
	KERNEL_TRACE("sched", "being asked to schedule someone else. in_kernel=%d. have %d choices.",
				 in_kernel,
				 act_queue_end);
	if(hint != NULL) {
		KERNEL_TRACE("sched", "hint is %s", hint->name);
	}

	if(!hint || hint->sched_status != sched_runnable) {
		hint = sched_picknext();
	}

	if(!hint) {
		sched_nothing_to_run();
	} else if(hint != kernel_curr_act) {
		act_t* from = kernel_curr_act;
		act_t* to = hint;

		kernel_critical_section_enter();
		sched_deschedule(from, into_state);
		sched_schedule(to);
		if(!in_kernel) {
			/* We are here on the users behalf, so our context will not be restored from the exception_frame_ptr */
			/* swap state will exit ALL the critical sections and will seem like a no-op from the users perspective */

			swap_state(&from->saved_registers, &to->saved_registers);

		} else {
			kernel_critical_section_exit();
		}

	}

}