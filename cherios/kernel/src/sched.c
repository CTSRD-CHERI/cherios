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
#ifdef K_DEBUG
#ifdef HARDWARE_fpga
#include "statcounters.h"
#endif
#endif

/* todo: sleep cpu */
static act_t idle_acts[SMP_CORES];

static void sched_nothing_to_run(void) __dead2;
static void sched_nothing_to_run(void) {
	KERNEL_ERROR("No activation to schedule");

    for(act_t* act = act_list_start; act != NULL; act = act->list_next) {
        kernel_printf("%20s : status %x\n", act->name, act->sched_status);
    }

	kernel_freeze();
}

#define SCHED_QUEUE_LENGTH 0x10
#define LEVEL_TO_NDX(level) ((level > PRIO_IO) ? PRIO_IO : level)


typedef struct sched_q {
	act_t* 		act_queue[SCHED_QUEUE_LENGTH];
	size_t   	act_queue_current; // index round robin. MAY NOT ACTUALLY BE CURRENT;
	size_t   	act_queue_end;	   // index for end;
	uint8_t		queue_ctr;		   // a counter for how many tasks at this level have been processed
} sched_q;

typedef struct sched_pool {
	/* The currently scheduled activation */
	act_t*		idle_act;
	act_t* 		current_act; // DONT use the current index for this. This can be accessed without a lock.
	sched_q 	queues[SCHED_PRIO_LEVELS];
	size_t 		in_queues;
	spinlock_t 	queue_lock;
    uint8_t     pool_id;
#if (K_DEBUG)
    uint32_t    last_time;
    STAT_DEBUG_LIST(STAT_MEMBER)
#endif
} sched_pool;

sched_pool sched_pools[SMP_CORES];

#define FOREACH_POOL(p) for(sched_pool* p = sched_pools; p != (sched_pools + SMP_CORES); p++)

void sched_init(sched_idle_init_t* sched_idle_init) {
    uint8_t i = 0;
	FOREACH_POOL(pool) {
		spinlock_init(&pool->queue_lock);
		pool->current_act = NULL;
		for(size_t j = 0; j != SCHED_PRIO_LEVELS; j++) {
			pool->queues[j].act_queue_current = 0;
			pool->queues[j].act_queue_end = 0;
			pool->queues[j].queue_ctr = 0;
		}
		pool->in_queues = 0;
		pool->idle_act = NULL;
        pool->pool_id = i;

#if (K_DEBUG)
        pool->last_time = (uint32_t)cp0_count_get();
#endif
        capability qsz_cap = & pool->in_queues;
        qsz_cap = cheri_setbounds(qsz_cap, sizeof(size_t));
        qsz_cap = cheri_andperm(qsz_cap, CHERI_PERM_LOAD);

        sched_idle_init->queue_fill_pre[i] = qsz_cap;
        i++;
	}
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
	sched_q* q = &pool->queues[LEVEL_TO_NDX(act->priority)];
	kernel_assert(q->act_queue_end != SCHED_QUEUE_LENGTH);
    kernel_assert(!act->is_idle);
	spinlock_acquire(&pool->queue_lock);
	q->act_queue[q->act_queue_end++] = act;
	act->sched_status = set_to;
	pool->in_queues++;
 	spinlock_release(&pool->queue_lock);
}

static void delete_act_from_queue(sched_pool* pool, act_t * act, sched_status_e set_to, size_t index_hint) {
    size_t index;

	kernel_assert(!act->is_idle);

	restart: index = 0;
	sched_q* q = &pool->queues[LEVEL_TO_NDX(act->priority)];

	if(q->act_queue[index_hint] == act) {
		index = index_hint;
	} else if (q->act_queue[q->act_queue_current] == act) {
		index = q->act_queue_current;
	} else {
		size_t i;
		for(i = 0; i < q->act_queue_end; i++) {
			if(q->act_queue[i] == act) {
				index = i;
				break;
			}
		}
	}

	spinlock_acquire(&pool->queue_lock);
	if(q->act_queue[index] != act) {
		spinlock_release(&pool->queue_lock);
		goto restart;
	}
	q->act_queue_end--;
	q->act_queue[index] = q->act_queue[q->act_queue_end];
	q->act_queue[q->act_queue_end] = NULL;

	act->sched_status = set_to;
	pool->in_queues--;
	spinlock_release(&pool->queue_lock);
}

static act_t* get_idle(sched_pool* pool) {
    return pool->idle_act;
}

void sched_create(uint8_t pool_id, act_t * act, enum sched_prio priority) {
	KERNEL_TRACE("sched", "create %s in pool %d", act->name, pool_id);
	spinlock_init(&act->sched_access_lock);
	act->priority = priority;
	act->is_idle = 0;
	if(act->status == status_alive) {
		KERNEL_TRACE("sched", "add %s  - adding to pool %x", act->name, pool_id);
        // FIXME: A bit of a hack
        if(strcmp(act->name, "idle.elf") == 0) {
            static uint8_t idles_registered = 0;
            act->sched_status = sched_runnable;
            act->pool_id = idles_registered;
            sched_pools[idles_registered].idle_act = act;
			act->is_idle = 1;
            act->priority = PRIO_IDLE;
#ifdef SMP_ENABLED
            if(idles_registered != 0) {
                act->sched_status = sched_running;
				// Schedule the idle process when created for cores other than 0.
                kernel_assert(sched_pools[idles_registered].current_act == NULL);
                sched_pools[idles_registered].current_act = act;
                int result = smp_context_start(act->context, idles_registered);
            }
#endif
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

void sched_change_prio(act_t* act, enum sched_prio new_prio) {
	if(act->priority != new_prio) {
		uint8_t out_pool_id; // This is the pool id of the currently running thing
		CRITICAL_LOCKED_BEGIN_ID(&act->sched_access_lock, out_pool_id);
		if(act->sched_status <= sched_running) {
			delete_act_from_queue(&sched_pools[act->pool_id], act, act->sched_status, 0);
			act->priority = new_prio;
			add_act_to_queue(&sched_pools[act->pool_id], act, act->sched_status);
		} else {
			act->priority = new_prio; // If blocked this wont be in queue;
		}
		CRITICAL_LOCKED_END(&act->sched_access_lock);
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
	/* Set condition variable */
	if(events & sched_sync_block) {
		kernel_assert(act->sync_state.sync_condition == 1);
		act->sync_state.sync_condition = 0;
	}
	if(act->sched_status & events) {
		if(act->sched_status & sched_wait_timeout) {
			kernel_timer_unsubcsribe(act);
		}
		add_act_to_queue(&sched_pools[act->pool_id], act, sched_runnable);
	} else {
        if(events & sched_wait_notify) {
            act->early_notify = 1;
        }
        if(events & sched_waiting) {
            act->commit_early_notify = 1;
        }
    }
	CRITICAL_LOCKED_END(&act->sched_access_lock);
}
/* This will block until ANY of the events specified by events occurs */

register_t sched_block_until_event(act_t* act, act_t* next_hint, sched_status_e events, register_t timeout, int in_exception_handler) {

    if(act == NULL) act = sched_get_current_act();
	if(timeout > 0) {
		events |= sched_wait_timeout;
	}
    int got_event = 0;

    CRITICAL_LOCKED_BEGIN(&act->sched_access_lock);

    if((events & sched_sync_block) && !act->sync_state.sync_condition) got_event = 1;
    if((events & sched_waiting) && !msg_queue_empty(act)) got_event = 1;
    if((events & sched_wait_commit) && act->commit_early_notify) got_event = 1;
    if((events & sched_wait_notify) && act->early_notify) {
        act->early_notify = 0;
        got_event = 1;
    }
    if(events & sched_sem) kernel_panic("Not implemented");

    if(!got_event) {
		if(timeout > 0) kernel_timer_subscribe(act, timeout);
		else kernel_timer_start_count(act);
        sched_block(act, events);
		act->priority |= PRIO_IO;
    }

    CRITICAL_LOCKED_END(&act->sched_access_lock);

    if(!got_event) {
		sched_reschedule(next_hint, in_exception_handler);
		return (get_high_res_time(cp0_get_cpuid()) - act->timeout_start);
	}

	return 0;
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

	if(pool->in_queues == 0) {
		spinlock_release(&pool->queue_lock);
		return get_idle(pool);
	}

	uint8_t index = LEVEL_TO_NDX(PRIO_IO);
	uint8_t index_found = LEVEL_TO_NDX(PRIO_IDLE);

	// first select index from IO to LOW giving exponentially more priority to higher levels
	while(index != LEVEL_TO_NDX(PRIO_LOW) &&
			(pool->queues[index].act_queue_end == 0 ||
					(index_found = index, ((pool->queues[index].queue_ctr++) & (SCHED_PRIO_FACTOR-1))) == 0))
							index--;
	// And only selecting PRIO_IDLE if no other priority was found
	if(pool->queues[index].act_queue_end == 0) index = index_found;

	kernel_assert(pool->queues[index].act_queue_end != 0);

	sched_q* q = &pool->queues[index];

	q->act_queue_current = (q->act_queue_current + 1) % q->act_queue_end;
	act_t * next = q->act_queue[q->act_queue_current];

	kernel_assert(next != NULL);
    kernel_assert(LEVEL_TO_NDX(next->priority) == index);
	// FIXME: I am worried about this ordering. If deadlock. look here.
	spinlock_acquire(&next->sched_access_lock);
	spinlock_release(&pool->queue_lock);

	if(!(next->sched_status == sched_runnable || next->sched_status == sched_running)) {
		kernel_printf("Activation %s is in the queue and is not runnable\n", next->name);
        kernel_printf("%p: guard %lx. nxt %p. status %d. queue %p, mask %x\n", next, next->ctl.guard.guard, next->list_next,
                      next->sched_status, next->msg_queue, next->queue_mask);
		kernel_assert(0);
	}

	if(next->priority & PRIO_IO) {
		// IO priority gets scheduled once then set back to normal priority
		delete_act_from_queue(pool, next, next->sched_status, q->act_queue_current);
		next->priority &= ~PRIO_IO;
		add_act_to_queue(pool, next, next->sched_status);
	}

	return next;
}

// This will cause a switch if the activation that recieves an interrupt is on the current core
// and whatever is currently scheduled does not have high priority
void sched_got_int(act_t* act, uint8_t cpu_id) {
	spinlock_acquire(&act->sched_access_lock);
	int should_switch = (act->pool_id == cpu_id) &&
			(sched_pools[cpu_id].current_act->priority != PRIO_IO) &&
			(act->sched_status == sched_runnable);
	spinlock_release(&act->sched_access_lock);
	if(should_switch) {
		sched_reschedule(act, 1);
	}
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

#if (K_DEBUG)
        uint32_t last_time = pool->last_time;
        uint32_t now = (uint32_t)cp0_count_get();
#define GET_STAT(item, ...) uint64_t item = get_ ## item ##_count();
#define INC_STAT(item, ...) kernel_curr_act->item += (item - pool->item);
#define SET_STAT(item, ...) pool->item = item;

        STAT_DEBUG_LIST(GET_STAT)

        if(kernel_curr_act) {
            //resetStatCounters(); reset is a lie for some of the stats. Track ourselves.
			STAT_DEBUG_LIST(INC_STAT)
            kernel_curr_act->had_time += now - last_time;
            kernel_curr_act->had_time_epoch += now - last_time;
        }
        pool->last_time = now;
        STAT_DEBUG_LIST(SET_STAT)
#endif

		if(hint != kernel_curr_act) {

			act_t* from = kernel_curr_act;
			act_t* to = hint;

#if (K_DEBUG)
			to->switches++;
#endif
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
                context_switch(to->context);
            }
		} else {
			spinlock_release(&hint->sched_access_lock);
			if(!in_exception_handler) {
				critical_section_exit();
			}
        }
	}
}