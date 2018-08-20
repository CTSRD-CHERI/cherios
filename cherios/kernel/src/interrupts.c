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

static interrupt_register_t int_child[INTERRUPTS_N*SMP_CORES];
static uint64_t masks[SMP_CORES];

#define GET_REG(core,id) int_child[id + (core * INTERRUPTS_N)]

/* Does NOT include IM7 (timer) which is handled directly by the kernel */

// WARN: We are ignoring cpu numbers in qemu. Only 0 will work. For BERI PIC interrupts will work correctly

/*
 *
 * Will not actually be accessible. This is for debug purposes.
uint64_t* pic_config;
uint64_t* pic_read;

void dump_config(size_t index, uint64_t config) {
    kernel_printf("%4ld: %lx : (%lx,%ld)\n", index, config, config & ((1 << PIC_CONFIG_SIZE_IRQ)-1), (config >> PIC_CONFIG_OFFSET_E) & 1);
}

void dump_pic(void) {

    size_t i;

    for(i = 0; i != 8; i++) {
        dump_config(i, pic_config[i]);
    }

    kernel_printf("Read: %lx, %lx", pic_read[0], pic_read[1]);
}

void debug_pic(void) {
    page_t* book = get_book();

    pic_config = (uint64_t*)get_phy_cap(book, 0x7f804000, 1024, 0, 1);
    pic_read = (uint64_t*)get_phy_cap(book, 0x7f804000 + ((8 * 1024)), 128, 0 ,1);
}
*/

void kernel_interrupts_init(int enable_timer, uint8_t cpu_id) {
	KERNEL_TRACE("interrupts", "enabling interrupts");
	kernel_assert(cp0_status_ie_get() == 0);
	cp0_status_ie_enable();

	if(enable_timer) {
		/* Start timer */
		kernel_timer_init(cpu_id);
		register_t shifted = (1 << MIPS_CP0_STATUS_IM_SHIFT+MIPS_CP0_INTERRUPT_TIMER);
		modify_hardware_reg(NANO_REG_SELECT_STATUS, shifted, shifted);
	}
}

static void kernel_interrupt_others(register_t pending, uint8_t cpu_id) {
	for(register_t i=0; i<INTERRUPTS_N; i++) {
		if(pending & (1L<<i)) {
			interrupt_register_t* registration = &GET_REG(cpu_id,i);
			if(registration->target == NULL) {
				KERNEL_ERROR("Interrupt with no handle %ld",i);
				continue;
			}
            KERNEL_TRACE("interrupt disable", "%ld", i);

			interrupts_mask(cpu_id, i, 0);
			masks[cpu_id] &=~(1L << i);
			// FIXME we probabably want a seperate interrupt source from the kernel

			// FIXME this breaks as we can't take a proper TLB fault in exception levels
			// FIXME I have added a cludge to load every slot on register but this needs fixing
			if(msg_push(registration->carg, NULL, NULL, NULL,
						registration->arg, i, 0, 0,
						registration->v0, registration->target, &kernel_acts[0], NULL)) {
                kernel_printf(KRED"Could not send interrupt to %s. Queue full\n"KRST, registration->target->name);
			}

            // DOES NOT WAKE UP. This is to go to higher priority as the quantum are currently very long
            sched_got_int(registration->target, cpu_id);
		}
	}
}

void kernel_interrupt(register_t cause, uint8_t cpu_id) {
	uint64_t ipending = interrupts_get(cpu_id);
	register_t mask = masks[cpu_id];

	register_t toprocess = ipending & mask;

    register_t handle_time = cause & (1 << (MIPS_CP0_INTERRUPT_TIMER + MIPS_CP0_STATUS_IM_SHIFT));

	KERNEL_TRACE("interrupt", "pending: %lx, to_process: %lx cpu: %d ", ipending, toprocess, cpu_id);

    kernel_assert(handle_time || toprocess);

	if (handle_time) {
		kernel_timer(cpu_id);
	}
	if(toprocess) {
		kernel_interrupt_others(toprocess, cpu_id);
	}
}

static int validate_number(int number) {
	if(number<0 || number>=INTERRUPTS_N) {
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

	masks[cpu_id] |= 1L << number;
	interrupts_mask(cpu_id, number, 1);

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
