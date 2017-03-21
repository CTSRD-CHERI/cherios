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

static void kernel_exception_capability(void) {
	cap_exception_t exception = parse_cause(cheri_getcause());

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
	exception_printf(KRED"Data abort type %d, BadVAddr:0x%lx in %s"KRST"\n",
	       excode, cp0_badvaddr_get(),
	       kernel_curr_act->name);
	regdump(-1);
	kernel_freeze();
}

static void kernel_exception_trap() {
	exception_printf(KRED"trap in %s"KRST"\n"
					 , kernel_curr_act->name);
	regdump(-1);
	kernel_freeze();
}

static void kernel_exception_unknown(register_t excode) __dead2;
static void kernel_exception_unknown(register_t excode) {
	exception_printf(KRED"Unknown exception type '%d' in  %s"KRST"\n",
	       excode, kernel_curr_act->name);
	regdump(-1);
	kernel_freeze();
}


/*
 * Exception handler demux to various more specific exception
 * implementations.
 */
void kernel_exception(context_t swap_to, context_t own_context) {
	context_t victim_context = swap_to;
	context_t own_save; // We never use this, there is currently no reason to restore the exception context

	set_exception_handler(own_context);
	cp0_status_bev_set(1);
	kernel_interrupts_init(1);

	while(1) {
		context_switch(victim_context, &own_save);
		// We will next to be switched to with c3 containing a victim context
		__asm__ __volatile__ ("cmove %[x], $c3" : [x]"=C"(victim_context));

		static int entered = 0;
		entered++;

		KERNEL_TRACE("exception", "saving %s",
					 kernel_curr_act->name);
		KERNEL_TRACE("exception", "enters %d", entered);
		if(entered > 1) {
			KERNEL_ERROR("interrupt in interrupt");
			kernel_freeze();
		}

		/*
         * Check assumption that kernel is running at EXL=1.  The kernel is
         * non-preemptive and will fail horribly if this isn't true.
         */
		kernel_assert(cp0_status_exl_get() != 0);

		register_t excode = cp0_cause_excode_get();
		switch (excode) {
			case MIPS_CP0_EXCODE_INT:
				kernel_interrupt();
				break;

			case MIPS_CP0_EXCODE_SYSCALL:
				exception_printf(KRED"Synchronous syscalls now use the ccall interface"KRST"\n");
				regdump(-1);
				kernel_freeze();
				break;

			case MIPS_CP0_EXCODE_C2E:
				kernel_exception_capability();
				break;

			case MIPS_CP0_EXCODE_TLBL:
			case MIPS_CP0_EXCODE_TLBS:
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

		KERNEL_TRACE("exception", "returns %d", entered);
		entered--;
		if(entered) {
			KERNEL_ERROR("interrupt in interrupt");
			kernel_freeze();
		}

		KERNEL_TRACE("exception", "restoring %s",
					 kernel_curr_act->name);
	}
}
