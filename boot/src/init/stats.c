/*-
 * Copyright (c) 2016 Hongyan Xia
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

#include "boot/boot.h"
#include "statcounters.h"

#ifdef HARDWARE_fpga
	#define USE_STATCOUNTERS
#endif

#ifdef USE_STATCOUNTERS
static statcounters_bank_t counter_start_bank;
static statcounters_bank_t counter_end_bank;
static statcounters_bank_t counter_diff_bank;

static statcounters_bank_t * counter_start = &counter_start_bank;
static statcounters_bank_t * counter_end   = &counter_end_bank;
static statcounters_bank_t * counter_diff  = &counter_diff_bank;
#endif

void stats_init(void) {
	#ifdef USE_STATCOUNTERS
	/* Reset the statcounters */
	reset_statcounters();
	zero_statcounters(counter_start);
	zero_statcounters(counter_end);
	zero_statcounters(counter_diff);

	/* Start sample */
	sample_statcounters(counter_start);
	#endif
}

void stats_display(void) {
	#ifdef USE_STATCOUNTERS
	sample_statcounters(counter_end);
	diff_statcounters(counter_end, counter_start, counter_diff);
	dump_statcounters(counter_diff, NULL, NULL);
	#endif
}
