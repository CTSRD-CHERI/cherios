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

#include "activations.h"
#include "klib.h"
#include "cp0.h"
static act_t * int_child[7];

/* Does NOT include IM7 (timer) which is handled directly by the kernel */
int get_others_interrupts_mask(void) {
	register_t im;

	im = cp0_status_get();
	im >>= MIPS_CP0_STATUS_IM_SHIFT;
	im &= 0x7F;
	return im;
}

void kernel_interrupts_init(int enable_timer) {
	KERNEL_TRACE("init", "enabling interrupts");
	kernel_assert(cp0_status_ie_get() == 0);
	cp0_status_ie_enable();

	if(enable_timer) {
		/* Start timer */
		kernel_timer_init();
		cp0_status_im_enable(MIPS_CP0_STATUS_IM_TIMER);
	}
}

static void kernel_interrupt_others(register_t pending) {
	for(size_t i=0; i<7; i++) {
		if(pending & (1<<i)) {
			if(int_child[i] == NULL) {
				KERNEL_ERROR("unknown interrupt %lx", i);
				continue;
			}
			cp0_status_im_disable(1<<i);
			// FIXME we probabably want a seperate interrupt source from the kernel
			if(msg_push(NULL, NULL, NULL, i, 0, 0, -3, int_child[i], &kernel_acts[0], NULL)) {
				kernel_panic("queue full (int)");
			}
		}
	}
}

void kernel_interrupt(void) {
	register_t ipending = cp0_cause_ipending_get();
	register_t toprocess = ipending & get_others_interrupts_mask();
	KERNEL_TRACE("interrupt", "%lx %lx", ipending, toprocess);
	if (ipending & MIPS_CP0_CAUSE_IP_TIMER) {
		kernel_timer();
	}
	if(toprocess) {
		kernel_interrupt_others(toprocess);
	}
}

static int validate_number(int number) {
	if(number<0 || number>7) {
		return -1;
	}
	return number;
}

int kernel_interrupt_enable(int number, act_control_t * ctrl) {
	number = validate_number(number);
	if(number < 0) {
		return -1;
	}
	if(int_child[number] != ctrl) {
		return -1;
	}
	cp0_status_im_enable(1<<number);
	return 0;
}

int kernel_interrupt_register(int number, act_control_t * ctrl) {
	number = validate_number(number);
	if(number < 0) {
		return -1;
	}
	if(int_child[number] != NULL) {
		return -1;
	}
	int_child[number] = (act_t*)ctrl;
	return 0;
}
