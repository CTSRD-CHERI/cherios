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

#include <queue.h>
#include "activations.h"
#include "klib.h"
#include "cp0.h"

typedef struct interrupt_register_t {
	act_t* target;
	register_t v0;
	capability carg;
	register_t arg;
} interrupt_register_t;

static interrupt_register_t int_child[7*SMP_CORES];

#define GET_REG(core,id) int_child[id + (core * SMP_CORES)]

/* FIXME This entire thing will break when remove access from the kernel to CP0
 * FIXME the solution will be to have the nano kernel expose a suitable interface

/* Does NOT include IM7 (timer) which is handled directly by the kernel */

void kernel_interrupts_init(int enable_timer, uint8_t cpu_id) {
	KERNEL_TRACE("interrupts", "enabling interrupts");
	kernel_assert(cp0_status_ie_get() == 0);
	cp0_status_ie_enable();

	if(enable_timer) {
		/* Start timer */
		kernel_timer_init(cpu_id);
		cp0_status_im_enable(MIPS_CP0_STATUS_IM_TIMER);
	}
}

static void kernel_interrupt_others(register_t pending, uint8_t cpu_id) {
	for(uint8_t i=0; i<7; i++) {
		if(pending & (1<<i)) {
			interrupt_register_t* registration = &GET_REG(cpu_id,i);
			if(registration->target == NULL) {
				KERNEL_ERROR("Interrupt with no handle %d",i);
				continue;
			}
            KERNEL_TRACE("interrupt disable", "%d", i);
			cp0_status_im_disable(1<<i);
			// FIXME we probabably want a seperate interrupt source from the kernel

			// FIXME this breaks as we can't take a proper TLB fault in exception levels
			// FIXME I have added a cludge to load every slot on register but this needs fixing
			if(msg_push(registration->carg, NULL, NULL, NULL,
						registration->arg, i, 0, 0,
						registration->v0, registration->target, &kernel_acts[0], NULL)) {
                kernel_printf(KRED"Could not send interrupt to %s. Queue full"KRST, registration->target->name);
			}
		}
	}
}

void kernel_interrupt(register_t cause, uint8_t cpu_id) {
	register_t ipending = cp0_cause_ipending_get(cause);
	register_t mask = cp0_status_im_get();
	register_t toprocess = ipending & (~MIPS_CP0_CAUSE_IP_TIMER) & mask;

	KERNEL_TRACE("interrupt", "pending: %lx, to_process: %lx cpu: %d ", ipending, toprocess, cpu_id);
	if (ipending & MIPS_CP0_CAUSE_IP_TIMER) {
		kernel_timer(cpu_id);
	}
	if(toprocess) {
		kernel_interrupt_others(toprocess, cpu_id);
	}
}

static int validate_number(int number) {
	if(number<0 || number>7) {
		return -1;
	}
	return number;
}

int kernel_interrupt_enable(int number, act_control_t * ctrl) {
	// FIXME races
    KERNEL_TRACE("interrupt enable", "%d", number);
	number = validate_number(number);
	if(number < 0) {
		return -1;
	}
	uint8_t cpu_id = cp0_get_cpuid();
	if(GET_REG(cpu_id,number).target != ctrl) {
		return -1;
	}
    // This will eventually fail when we can no longer access cp0
	cp0_status_im_enable(1<<number);
	return 0;
}

int kernel_interrupt_register(int number, act_control_t * ctrl, register_t v0, register_t arg, capability carg) {
	// FIXME races
	number = validate_number(number);
	if(number < 0) {
		return -1;
	}
	uint8_t cpu_id = cp0_get_cpuid();
	if(GET_REG(cpu_id,number).target != NULL && GET_REG(cpu_id,number).target != ctrl) {
		return -1;
	}

	// Hack to make sure TLB entries exist
	queue_t* q = ctrl->msg_queue;
	for(size_t i = 0; i < ctrl->queue_mask+1; i++) {
		volatile char* f = (char*)&q->msg[i].c3;
		*f;
		f = (char*)&q->msg[i].v0;
		*f;
	}


	GET_REG(cpu_id,number).target = (act_t*)ctrl;
	GET_REG(cpu_id,number).arg = arg;
	GET_REG(cpu_id,number).carg = carg;
	GET_REG(cpu_id,number).v0 = v0;
	return 0;
}
