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

#include "mutex.h"
#include "activations.h"
#include "klib.h"
#include "nano/nanokernel.h"

/* todo: sleep cpu */
static void sched_nothing_to_run(void) __dead2;
static void sched_nothing_to_run(void) {
	KERNEL_ERROR("No activation to schedule");

    for(act_t* act = act_list_start; act != NULL; act = act->list_next) {
        kernel_printf("%20s : status %d\n", act->name, act->sched_status);
    }

	kernel_freeze();
}

#define SCHED_QUEUE_LENGTH 0x10

static act_t * act_queue[SCHED_QUEUE_LENGTH];
static size_t   act_queue_current = 0;
static size_t   act_queue_end = 0;

spinlock_t queue_lock;

void sched_init() {
	spinlock_init(&queue_lock);
}

static void add_act_to_queue(act_t * act, sched_status_e set_to) {
	kernel_assert(act_queue_end != SCHED_QUEUE_LENGTH);
	CRITICAL_LOCKED_BEGIN(&queue_lock);
	act_queue[act_queue_end++] = act;
	act->sched_status = set_to;
	CRITICAL_LOCKED_END(&queue_lock);
}

static void delete_act_from_queue(act_t * act, sched_status_e set_to) {
    size_t index;
	restart: index = 0;
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

	CRITICAL_LOCKED_BEGIN(&queue_lock);
	if(act_queue[index] != act) {
		CRITICAL_LOCKED_END(&queue_lock);
		goto restart;
	}
	act_queue_end--;
	act_queue[index] = act_queue[act_queue_end];
	act->sched_status = set_to;
	CRITICAL_LOCKED_END(&queue_lock);
}

void sched_create(act_t * act) {
	KERNEL_TRACE("sched", "create %s", act->name);
	if(act->status == status_alive) {
		KERNEL_TRACE("sched", "add %s", act->name);
		add_act_to_queue(act, sched_runnable);
	} else {
		act->sched_status = sched_terminated;
	}
}

void sched_delete(act_t * act) {
	KERNEL_TRACE("sched", "delete %s", act->name);
	int deleted_self = act->sched_status == sched_running;

	if(act->sched_status == sched_runnable || act->sched_status == sched_running) {
		delete_act_from_queue(act, sched_terminated);
	}

	act->status = status_terminated;

	if(deleted_self) {
		sched_reschedule(NULL, 0);
	} else {
		act->sched_status = sched_terminated;
	}
}

void sched_receives_sem_signal(act_t * act) {
	kernel_assert(act->sched_status == sched_sem);
	KERNEL_TRACE("sched", "now unblocked on sempahore%s", act->name);
	add_act_to_queue(act, sched_runnable);
}

void sched_receives_msg(act_t * act) {
	if(act->sched_status == sched_waiting) {
		KERNEL_TRACE("sched", "now unblocked %s", act->name);
		add_act_to_queue(act, sched_runnable);
	}
}

void sched_recieve_ret(act_t * act) {
	kernel_assert(act->sched_status == sched_sync_block);
	KERNEL_TRACE("sched", "now unblocked %s", act->name);
	add_act_to_queue(act, sched_runnable);
}

void sched_block_until_msg(act_t * act, act_t * next_hint) {
	kernel_assert(act->sched_status == sched_running);

	CRITICAL_LOCKED_BEGIN(&act->writer_spinlock);

	if(msg_queue_empty(act)) {
		sched_block(act, sched_waiting);
		CRITICAL_LOCKED_END(&act->writer_spinlock);
		/* Somebody may now send a message. This is ok, we will get back in the queue */
		sched_reschedule(next_hint, 0);
	} else {
		CRITICAL_LOCKED_END(&act->writer_spinlock);
	}
}

void sched_block(act_t *act, sched_status_e status) {
	KERNEL_TRACE("sched", "blocking %s", act->name);
	kernel_assert((status == sched_sync_block) || (status == sched_waiting) || (status == sched_sem));

	if(act->sched_status == sched_runnable || act->sched_status == sched_running) {
		delete_act_from_queue(act, status);
	} else {
		act->sched_status = status;
	}
}

static void sched_deschedule(act_t * act) {
	/* The caller should have put this in a sensible state if wanted something different */
	if(act->sched_status == sched_running) {
		act->sched_status = sched_runnable;
	}
	KERNEL_TRACE("sched", "Reschedule from activation '%s'", act->name);
}

void sched_schedule(act_t * act) {
	kernel_assert(act->sched_status == sched_runnable);
	act->sched_status = sched_running;
	kernel_curr_act = act;
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
	if(!(next->sched_status == sched_runnable || next->sched_status == sched_running)) {
		kernel_printf("Activation %s is in the queue and is not runnable\n", next->name);
        kernel_printf("%p: guard %lx. nxt %p. prev %p. status %d. queue %p, mask %lx\n", next, next->stack_guard, next->list_next,
         next->list_prev, next->sched_status, next->msg_queue, next->queue_mask);
		kernel_assert(0);
	}

	return next;
}

void sched_reschedule(act_t *hint, int in_exception_handler) {
	KERNEL_TRACE("sched", "being asked to schedule someone else. in_exception_handler=%d. have %lu choices.",
				 in_exception_handler,
				 act_queue_end);

	if(hint != NULL) {
		KERNEL_TRACE("sched", "hint is %s", hint->name);
	}

	if(!hint || hint->sched_status != sched_runnable) {
		hint = sched_picknext();
	}

	if(!hint) {
		sched_nothing_to_run();
	} else {

        if(!in_exception_handler) {
            FAST_CRITICAL_ENTER
        }

		if(hint != kernel_curr_act) {

			act_t* from = kernel_curr_act;
			act_t* to = hint;

			sched_deschedule(from);
			sched_schedule(to);

			if(!in_exception_handler) {
				if(from->status == status_terminated) {
					KERNEL_TRACE("sched", "now destroying %s", from->name);
					destroy_context(from->context, to->context); // This will never return
				}
				/* We are here on the users behalf, so our context will not be restored from the exception_frame_ptr */
				/* swap state will exit ALL the critical sections and will seem like a no-op from the users perspective */
				context_switch(to->context, &from->context);
			}
		} else {
            FAST_CRITICAL_EXIT
        }
	}
}