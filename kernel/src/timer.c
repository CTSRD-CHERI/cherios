/*-
 * Copyright (c) 2011 Robert N. M. Watson
 * Copyright (c) 2016 Hadrien Barral
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

static register_t		kernel_last_timer;
//#define	DEIMOS_TIMER_INTERVAL		400000
//#define	DEIMOS_TIMER_INTERVAL_MIN	30000
#define	DEIMOS_TIMER_INTERVAL		40000000
#define	DEIMOS_TIMER_INTERVAL_MIN	20000000

void kernel_timer_init(void) {
	/*
	 * Start timer.
	 */
	KERNEL_TRACE("boot", "starting timer");
	kernel_last_timer = cp0_count_get();
	kernel_last_timer += DEIMOS_TIMER_INTERVAL;
	cp0_compare_set(kernel_last_timer);

	/*
	 * Enable interrupts only once we're executing the kernel.
	 * EXL is still set so we can't be preempted.
	 */
	KERNEL_TRACE("boot", "enabling interrupts");
	kernel_assert(cp0_status_ie_get() == 0);
	cp0_status_ie_enable(); //move elsewhere?
	cp0_status_im_enable(MIPS_CP0_STATUS_IM_TIMER);
}

/*
 * Kernel timer handler -- reschedule, reset timer.
 */
void kernel_timer(void)
{
	KERNEL_TRACE(__func__, "in");

	/*
	 * Forced context switch of user process.
	 */
	kernel_reschedule();

	/*
	 * Reschedule timer for a future date -- if we've almost missed a
	 * tick, better to defer.
	 */
	register_t  next_timer = kernel_last_timer + DEIMOS_TIMER_INTERVAL;
	while (next_timer - DEIMOS_TIMER_INTERVAL_MIN < cp0_count_get()) {
		next_timer += DEIMOS_TIMER_INTERVAL;
	}
	cp0_compare_set(next_timer);		/* Clears pending interrupt. */

	kernel_last_timer = next_timer;
}
