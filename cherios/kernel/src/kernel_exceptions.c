/*-
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

#include <activations.h>
#include <nano/nanokernel.h>
#include "activations.h"
#include "klib.h"
#include "cp0.h"
#include "kernel_exceptions.h"

/*
 * Exception demux
 */

#ifndef HARDWARE_fpga
register_t badinstr_glob = 0;
#endif

DEFINE_ENUM_AR(cap_cause_exception_t, CAP_CAUSE_LIST)

static void kernel_exception_capability(register_t ccause) __dead2;
static void kernel_exception_capability(register_t ccause) {
	cap_exception_t exception = parse_cause(ccause);

	KERNEL_TRACE("exception", "kernel_capability %s", enum_cap_cause_exception_t_tostring(exception.cause));

	if(exception.cause == Call_Trap) {
		exception_printf("ccall No longer an exception\n");
	}
	if(exception.cause == Return_Trap) {
		exception_printf("creturn No longer an exception\n");
	}

	exception_printf(KRED "Capability exception caught in activation %s"
	             " (0x%X: %s) [Reg C%d]" KRST"\n",
	        kernel_curr_act->name,
		exception.cause, enum_cap_cause_exception_t_tostring(exception.cause), exception.reg_num);

	regdump(exception.reg_num);
	kernel_freeze();
}

static void kernel_exception_data(register_t excode) __dead2;
static void kernel_exception_data(register_t excode) {
	exception_printf(KRED"Data abort type %ld, BadVAddr:0x%lx in %s"KRST"\n",
	       excode, cp0_badvaddr_get(),
	       kernel_curr_act->name);
	regdump(-1);
	kernel_freeze();
}

static void kernel_exception_trap(void) __dead2;
static void kernel_exception_trap(void) {
	exception_printf(KRED"trap in %s"KRST"\n"
					 , kernel_curr_act->name);
	regdump(-1);
	kernel_freeze();
}

static void kernel_exception_unknown(register_t excode) __dead2;
static void kernel_exception_unknown(register_t excode) {
	exception_printf(KRED"Unknown exception type '%ld' in  %s"KRST"\n",
	       excode, kernel_curr_act->name);
	regdump(-1);
	kernel_freeze();
}


static void kernel_exception_tlb(register_t badvaddr);
static void kernel_exception_tlb(register_t badvaddr) {
	if(memgt_ref == NULL) {
		exception_printf(KRED"Virtual memory exception before memmgt created\n"KRST);
	}
	if(kernel_curr_act == memgt_ref) {
		exception_printf(KRED"Virtual memory exception in memmgt is not allowed\n"KRST);
		regdump(-1);
		kernel_freeze();
	}

	msg_push(act_create_sealed_ref(kernel_curr_act), NULL, NULL, NULL, badvaddr, 0, 0, 0, 2, memgt_ref, kernel_curr_act, NULL);
	sched_reschedule(memgt_ref, 1);
}
/*
 * Exception handler demux to various more specific exception
 * implementations.
 */
void kernel_exception(context_t swap_to, context_t own_context) {
    exection_cause_t ex_info;
    ex_info.victim_context = swap_to;

	context_t own_save; // We never use this, there is currently no reason to restore the exception context

	set_exception_handler(own_context);
	cp0_status_bev_set(0);
	kernel_interrupts_init(1);
	while(1) {
		KERNEL_TRACE("exception", "restoring %s", kernel_curr_act->name);
		context_switch(ex_info.victim_context, &own_save);

        get_last_exception(&ex_info);

		if(ex_info.victim_context != own_context) {
			// We only do this as handles are not guaranteed to stay fresh (although they are currently)
			kernel_curr_act->context = ex_info.victim_context;
		} else {
			// This happens if an interrupt happened during the exception level. As soon as we exit
			// Another exception happens and so take another.
		}


		static int entered = 0;
		entered++;

		register_t excode = cp0_cause_excode_get(ex_info.cause);

		KERNEL_TRACE("exception", "in %s. Enter: %d. Code %lx.",
					 kernel_curr_act->name,
					 entered,
					 (unsigned long)(ex_info.cause));

		if(entered > 1) {
			KERNEL_ERROR("interrupt in interrupt");
			kernel_freeze();
		}


		switch (excode) {
			case MIPS_CP0_EXCODE_INT:
				kernel_interrupt(ex_info.cause);
				break;

			case MIPS_CP0_EXCODE_SYSCALL:
				exception_printf(KRED"Synchronous syscalls now use the ccall interface"KRST"\n");
				regdump(-1);
				kernel_freeze();
				break;

			case MIPS_CP0_EXCODE_C2E:
				kernel_exception_capability(ex_info.ccause);
				break;
			case MIPS_CP0_EXCODE_TLBL:
			case MIPS_CP0_EXCODE_TLBS:
				kernel_exception_tlb(ex_info.badvaddr);
				break;
			case MIPS_CP0_EXCODE_ADEL:
			case MIPS_CP0_EXCODE_ADES:
			case MIPS_CP0_EXCODE_IBE:
			case MIPS_CP0_EXCODE_DBE:
				kernel_exception_data(excode);
				break;

			case MIPS_CP0_EXCODE_TRAP:
				kernel_exception_trap();
				break;
			default:
				kernel_exception_unknown(excode);
				break;
		}

		KERNEL_TRACE("exception", "restoring %s", kernel_curr_act->name);

		entered--;

		// We have changed context due to this exception, and so we should restore the current context
		ex_info.victim_context = kernel_curr_act->context;
	}
}
