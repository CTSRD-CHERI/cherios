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

#include <queue.h>
#include "sched.h"
#include "mutex.h"
#include "activations.h"
#include "klib.h"
#include "nano/nanokernel.h"
#include "cp0.h"
#include "boot_info.h"

/* todo: sleep cpu */
static act_t idle_acts[SMP_CORES];

static void sched_nothing_to_run(void) __dead2;
static void sched_nothing_to_run(void) {
	KERNEL_ERROR("No activation to schedule");

    for(act_t* act = act_list_start; act != NULL; act = act->list_next) {
        kernel_printf("%20s : status %d\n", act->name, act->sched_status);
    }

	kernel_freeze();
}

#define SCHED_QUEUE_LENGTH 0x10


typedef struct sched_pool {
	/* The currently scheduled activation */
	act_t*		idle_act;
	act_t* 		current_act; // DONT use the current index for this. This can be accessed without a lock.
	act_t* 		act_queue[SCHED_QUEUE_LENGTH];
	size_t   	act_queue_current; // index round robin. MAY NOT ACTUALLY BE CURRENT;
	size_t   	act_queue_end;	   // index for end;
	spinlock_t 	queue_lock;
    uint8_t     pool_id;
} sched_pool;

sched_pool sched_pools[SMP_CORES];

#define FOREACH_POOL(p) for(sched_pool* p = sched_pools; p != (sched_pools + SMP_CORES); p++)

void sched_init(sched_idle_init_t* sched_idle_init) {
    uint8_t i = 0;
	FOREACH_POOL(pool) {
		spinlock_init(&pool->queue_lock);
		pool->current_act = NULL;
		pool->act_queue_current = 0;
		pool->act_queue_end = 0;
		pool->idle_act = NULL;
        pool->pool_id = i;

        capability qsz_cap = & pool->act_queue_end;
        qsz_cap = cheri_setbounds(qsz_cap, sizeof(size_t));
        qsz_cap = cheri_andperm(qsz_cap, CHERI_PERM_LOAD);

        sched_idle_init->queue_fill_pre[i] = qsz_cap;
        i++;
	}
}

size_t* sched_get_queue_fill_pointer(uint8_t pool_id) {
	return &sched_pools[pool_id].act_queue_end;
}

act_t* sched_get_current_act_in_pool(uint8_t pool_id) {
	return sched_pools[pool_id].current_act;
}

act_t* sched_get_current_act(void) {
	uint8_t pool_id = critical_section_enter();
	act_t* ret = sched_get_current_act_in_pool(pool_id);
	critical_section_exit();
	return ret;
}

static void add_act_to_queue(sched_pool* pool, act_t * act, sched_status_e set_to) {
	kernel_assert(pool->act_queue_end != SCHED_QUEUE_LENGTH);
	spinlock_acquire(&pool->queue_lock);
	pool->act_queue[pool->act_queue_end++] = act;
	act->sched_status = set_to;
 	spinlock_release(&pool->queue_lock);
}

static void delete_act_from_queue(sched_pool* pool, act_t * act, sched_status_e set_to, size_t index_hint) {
    size_t index;
	restart: index = 0;

	if(pool->act_queue[index_hint] == act) {
		index = index_hint;
	} else if (pool->act_queue[pool->act_queue_current] == act) {
		index = pool->act_queue_current;
	} else {
		size_t i;
		for(i = 0; i < pool->act_queue_end; i++) {
			if(pool->act_queue[i] == act) {
				index = i;
				break;
			}
		}
	}

	spinlock_acquire(&pool->queue_lock);
	if(pool->act_queue[index] != act) {
		spinlock_release(&pool->queue_lock);
		goto restart;
	}
	pool->act_queue_end--;
	pool->act_queue[index] = pool->act_queue[pool->act_queue_end];
	pool->act_queue[pool->act_queue_end] = NULL;

	act->sched_status = set_to;
	spinlock_release(&pool->queue_lock);
}

static act_t* get_idle(sched_pool* pool) {
    return pool->idle_act;
}

void sched_create(uint8_t pool_id, act_t * act) {
	KERNEL_TRACE("sched", "create %s in pool %d", act->name, pool_id);
	spinlock_init(&act->sched_access_lock);
	if(act->status == status_alive) {
		KERNEL_TRACE("sched", "add %s  - adding to pool %x", act->name, pool_id);
        // FIXME: A bit of a hack
        if(strcmp(act->name, "idle.elf") == 0) {
            static uint8_t idles_registered = 0;
            act->sched_status = sched_runnable;
            act->pool_id = idles_registered;
            sched_pools[idles_registered].idle_act = act;

            if(idles_registered != 0) {
                act->sched_status = sched_running;
				// Schedule the idle process when created for cores other than 0.
                kernel_assert(sched_pools[idles_registered].current_act == NULL);
                sched_pools[idles_registered].current_act = act;
                int result = smp_context_start(act->context, idles_registered);
            }

            idles_registered++;
        } else {
            act->pool_id = pool_id;
			critical_section_enter();
            add_act_to_queue(&sched_pools[pool_id], act, sched_runnable);
			critical_section_exit();
        }
	} else {
		act->sched_status = sched_terminated;
	}
}

void sched_delete(act_t * act) {
	KERNEL_TRACE("sched", "delete %s", act->name);

	uint8_t pool_id;

	CRITICAL_LOCKED_BEGIN_ID(&act->sched_access_lock, pool_id);

	int deleted_running = act->sched_status == sched_running;
	int deleted_from_own_pool = act->pool_id == pool_id;

	kernel_assert((deleted_from_own_pool || !deleted_running) && "Need to send an interrupt to achieve this");

	if(act->sched_status == sched_runnable || act->sched_status == sched_running) {
		delete_act_from_queue(&sched_pools[act->pool_id], act, sched_terminated, 0);
	}

	act->sched_status = sched_terminated;

	spinlock_release(&act->sched_access_lock);

	if(deleted_running && deleted_from_own_pool) {
		sched_reschedule(NULL, 0);
	}

	critical_section_exit();
}

/* These should be called when an event is generated */

// This can be used to
void sched_receive_event(act_t* act, sched_status_e events) {
	CRITICAL_LOCKED_BEGIN(&act->sched_access_lock);
	if(act->sched_status & events) {
		if(act->sched_status & sched_wait_timeout) {
			kernel_timer_unsubcsribe(act);
		}
		add_act_to_queue(&sched_pools[act->pool_id], act, sched_runnable);
	} else if(events & sched_wait_notify) {
		act->early_notify = 1;
	}
	CRITICAL_LOCKED_END(&act->sched_access_lock);
}
/* This will block until ANY of the events specified by events occurs */

void sched_block_until_event(act_t* act, act_t* next_hint, sched_status_e events, register_t timeout) {

    if(act == NULL) act = sched_get_current_act();
	if(timeout > 0) {
		events |= sched_wait_timeout;
	}
    int got_event = 0;

    CRITICAL_LOCKED_BEGIN(&act->sched_access_lock);

    if((events & sched_sync_block) && !act->sync_state.sync_condition) got_event = 1;
    if((events & sched_waiting) && !msg_queue_empty(act)) got_event = 1;
    if((events & sched_wait_notify) && act->early_notify) {
        act->early_notify = 0;
        got_event = 1;
    }
    if(events & sched_sem) kernel_panic("Not implemented");

    if(!got_event) {
		if(timeout > 0) kernel_timer_subscribe(act, timeout);
        sched_block(act, events);
    }

    CRITICAL_LOCKED_END(&act->sched_access_lock);

    if(!got_event) sched_reschedule(next_hint, 0);
}

void sched_block(act_t *act, sched_status_e status) {
	KERNEL_TRACE("sched", "blocking %s", act->name);
	kernel_assert(status >= sched_waiting);

	if(act->sched_status == sched_runnable || act->sched_status == sched_running) {
		delete_act_from_queue(&sched_pools[act->pool_id], act, status, 0);
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

void sched_schedule(uint8_t pool_id, act_t * act) {
    KERNEL_TRACE("sched", "Reschedule to activation '%s'", act->name);
	kernel_assert(act->sched_status == sched_runnable);
	act->sched_status = sched_running;
	sched_pools[pool_id].current_act = act;
}

static act_t * sched_picknext(sched_pool* pool) {
	spinlock_acquire(&pool->queue_lock);

	if(pool->act_queue_end == 0) {
		spinlock_release(&pool->queue_lock);
		return get_idle(pool);
	}

	pool->act_queue_current = (pool->act_queue_current + 1) % pool->act_queue_end;
	act_t * next = pool->act_queue[pool->act_queue_current];

	// FIXME: I am worried about this ordering. If deadlock. look here.
	spinlock_acquire(&next->sched_access_lock);
	spinlock_release(&pool->queue_lock);

	if(!(next->sched_status == sched_runnable || next->sched_status == sched_running)) {
		kernel_printf("Activation %s is in the queue and is not runnable\n", next->name);
        kernel_printf("%p: guard %lx. nxt %p. status %d. queue %p, mask %x\n", next, next->ctl.guard.guard, next->list_next,
                      next->sched_status, next->msg_queue, next->queue_mask);
		kernel_assert(0);
	}

	return next;
}

void sched_reschedule(act_t *hint, int in_exception_handler) {

	uint8_t pool_id;

	if(!in_exception_handler) {
		pool_id = critical_section_enter();
	} else {
		pool_id = (uint8_t)cp0_get_cpuid();
	}

	sched_pool* pool = &sched_pools[pool_id];
	act_t* kernel_curr_act = pool->current_act;

    KERNEL_TRACE("sched", "being asked to schedule someone else in pool %d. in_exception_handler=%d. have %lu choices.",
                 pool_id,
                 in_exception_handler,
                 pool->act_queue_end);

	if(hint != NULL) {
		KERNEL_TRACE("sched", "hint is %s", hint->name);
		spinlock_acquire(&hint->sched_access_lock);
		if(hint->pool_id != pool_id) {
            HW_YIELD; // We yield because we probably want completion from the other core
			KERNEL_TRACE("sched", "hint was in another pool. Choosing another.");
		}

		if((hint->pool_id != pool_id) || (hint->sched_status != sched_runnable)) {
			spinlock_release(&hint->sched_access_lock);
			hint = NULL;
		}
	}

	if(!hint) {
		hint = sched_picknext(pool);
	}

	if(!hint) {
		sched_nothing_to_run();
	} else {

		if(hint != kernel_curr_act) {

			act_t* from = kernel_curr_act;
			act_t* to = hint;

			sched_deschedule(from);
			sched_schedule(pool_id, to);

            spinlock_release(&hint->sched_access_lock);

            if(!in_exception_handler) {
                if (from->status == status_terminated) {
                    KERNEL_TRACE("sched", "now destroying %s", from->name);
                    destroy_context(from->context, to->context); // This will never return
                }
                /* We are here on the users behalf, so our context will not be restored from the exception_frame_ptr */
                /* swap state will exit ALL the critical sections and will seem like a no-op from the users perspective */
                context_switch(to->context, &from->context);
            }
		} else {
			spinlock_release(&hint->sched_access_lock);
			if(!in_exception_handler) {
				critical_section_exit();
			}
        }
	}
}