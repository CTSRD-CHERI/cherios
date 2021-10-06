/*-
 * Copyright (c) 2011 Robert N. M. Watson
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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
#include "cp0.h"
#include "atomic.h"

// It _seems_ that even on QEMU the timers from cp0 stay in sync
// However, QEMU misses timer interrupts and so will have to wrap to take another, making these high res timers get
// out of sync. We could have only one core update the high res time, but then it might miss and time would stop
// for ~ 40s.

uint64_t high_resolution_timers[SMP_CORES];

static uint32_t kernel_last_timer[SMP_CORES];
// TODO everyone may wait on timeout, maybe just walk the list?
#define MAX_WAITERS 0x10
act_t* sleeps[MAX_WAITERS];
uint8_t init_sanity[SMP_CORES];

void kernel_timer_init(uint8_t cpu_id) {
	/*
	 * Start timer.
	 */
	KERNEL_TRACE("timer", "starting timer");
	uint8_t val;
	ATOMIC_ADD(&init_sanity[cpu_id], 8, 16i, 1, val);

	kernel_assert(val == 0);

	kernel_last_timer[cpu_id] = cp0_count_get();
	high_resolution_timers[cpu_id] = kernel_last_timer[cpu_id];
	kernel_last_timer[cpu_id] += TIMER_INTERVAL;
	cp0_compare_set(kernel_last_timer[cpu_id]);
}

uint64_t get_high_res_time(uint8_t cpu_id) {
	uint64_t old_high_res, new_high_res;
	register_t success;
	uint64_t* old_ptr = &high_resolution_timers[cpu_id];
	do {
		// Although we do not plan to update the high res counter here (we do that in a timer interrupt) we still need
		// to guard cp0_count_get inside a LL/SC otherwise we might double count the wrap around
        old_high_res = *old_ptr;
		uint32_t low_res = (uint32_t)cp0_count_get();
		// Ensure it hasnt changed
		ATOMIC_CAS(old_ptr, 64, old_high_res, old_high_res, success);
        if(success) {
            new_high_res = ((old_high_res >> 32) << 32) | (low_res);
            if(new_high_res < old_high_res) {
                if(old_high_res - new_high_res < 0x80000000ULL) {
                    kernel_printf(KRED"Timer has gone backwards by %lx ticks!\n"KRST, old_high_res - new_high_res);
                    new_high_res = old_high_res;
                } else {
                    new_high_res += 0x100000000ULL;
                }
            }
        }

	} while(!success);

	return new_high_res;
}

static void kernel_timer_check_sleepers(uint64_t now) {

	for(size_t i = 0; i < MAX_WAITERS; i++) {
		act_t* act = sleeps[i];

		if(act != NULL) {
			register_t waited = now - act->timeout_start;
#ifdef HARDWARE_qemu
            if((int64_t)waited < 0) continue; // Time going backwards can trigger this
#endif
			if(waited > act->timeout_length) {
                //if(act->name[0] == 'n') kernel_printf("%s has apparently waited %lx\n", act->name, waited);
				sched_receive_event(act, sched_wait_timeout);
			}
		}
	}
}

void kernel_timer_start_count(act_t* act) {
	act->timeout_start = get_high_res_time(cp0_get_cpuid()); // WARN: goes badly if we change cpu?
}

void kernel_timer_subscribe(act_t* act, register_t timeout) {
	kernel_timer_start_count(act);
	act->timeout_length = timeout;

    //if(act->name[0] == 'n') kernel_printf("%s Setting timeout for %lx\n", act->name, timeout);

	// Concurrent access
	register_t success = 0;
	for(size_t i = 0; (i < MAX_WAITERS) && !success; i++) {
		act_t** ptr = sleeps + i;
		act_t* a;
		do {
			LOAD_LINK(ptr, c, a);
			if(a != NULL) break;
			STORE_COND(ptr, c, act, success);
		} while (!success);

		if(success) {
            act->timeout_indx = i;
            kernel_assert(*ptr == act);
        }
	}

	kernel_assert(success && "This queue is too small\n");
}

void kernel_timer_unsubcsribe(act_t* act) {
	kernel_assert(sleeps[act->timeout_indx] == act);
	sleeps[act->timeout_indx] = NULL;
}
/*
 * Kernel timer handler -- reschedule, reset timer.
 */

void kernel_timer(uint8_t cpu_id)
{
	KERNEL_TRACE(__func__, "in %lu", cp0_count_get());

	// Set the high solution timer. This must be done before it wraps around since last call.
	__unused uint64_t old = high_resolution_timers[cpu_id];
	uint64_t new = get_high_res_time(cpu_id);
	kernel_assert(new > old);
    // FIXME QEMU is broken, it misses enough timer interrupts that this gets hit
    //kernel_assert(new - old <= (3 * TIMER_INTERVAL));
	high_resolution_timers[cpu_id] = new;

	kernel_timer_check_sleepers(new);

	/*
	 * Forced context switch of user process.
	 */
	sched_reschedule(NULL, 1);

	/*
	 * Reschedule timer for a future date -- if we've almost missed a
	 * tick, better to defer.
	 */
	/* count register is 32 bits */
	uint32_t next_timer = kernel_last_timer[cpu_id] + TIMER_INTERVAL;

// QEMU has timer jumps that can occur before compare set
#ifdef  HARDWARE_qemu
restart:;
#endif

    uint32_t cur = cp0_count_get();
    int32_t diff;

    // Catches either small, or negative timer offset
	while ((diff = (int32_t)next_timer - (int32_t)cur),(diff < TIMER_INTERVAL_MIN)) {
		next_timer = next_timer + TIMER_INTERVAL;
	}
	cp0_compare_set(next_timer);		/* Clears pending interrupt. */

#ifdef  HARDWARE_qemu
    uint32_t sanity = (uint32_t)cp0_count_get();
    diff = next_timer - sanity;
    if(diff < 0) goto restart;
#endif

	kernel_last_timer[cpu_id] = next_timer;
}
