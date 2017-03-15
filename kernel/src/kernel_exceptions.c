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

#include "klib.h"
#include "cp0.h"

/*
 * Exception demux
 */

#ifndef __LITE__
static const char * capcausestr[0x20] = {
	"None",
	"Length Violation",
	"Tag Violation",
	"Seal Violation",
	"Type Violation",
	"Call Trap",
	"Return Trap",
	"Underflow of trusted system stack",
	"User-defined Permission Violation",
	"TLB prohibits store capability",
	"Requested bounds cannot be represented exactly",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"Global Violation",
	"Permit Execute Violation",
	"Permit Load Violation",
	"Permit Store Violation",
	"Permit Load Capability Violation",
	"Permit Store Capability Violation",
	"Permit Store Local Capability Violation",
	"Permit Seal Violation",
	"Access System Registers Violation",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved"
};

#define exception_printf kernel_printf
#else
#define exception_printf(...)
#endif

static inline const char * getcapcause(int cause) {
	#ifndef __LITE__
		return capcausestr[cause];
	#else
		return ""; cause++;
	#endif
}

static void kernel_exception_capability(void) {
	KERNEL_TRACE("exception", "kernel_capability");
	register_t capcause = cheri_getcause();
	int cause = (capcause >> 8) & 0x1F;

	if(cause == 5) { /* todo: give them their own handler */
		kernel_ccall();
		return;
	}
	if(cause == 6) {
		kernel_creturn();
		return;
	}

	int reg_num = capcause & 0xFF;
	exception_printf(KRED "Capability exception catched for activation! %s-%d"
	             " (0x%X: %s) [Reg C%d]" KRST"\n",
	        kernel_acts[kernel_curr_act].name, kernel_curr_act,
		cause, getcapcause(cause), reg_num);

	regdump(reg_num);
	kernel_freeze();
}

static void kernel_exception_data(register_t excode) __dead2;

static void kernel_exception_data(register_t excode) {
	exception_printf(KRED"Data abort type %lu, BadVAddr:0x%lx in %s-%d\n",
	       excode, cp0_badvaddr_get(),
	       kernel_acts[kernel_curr_act].name, kernel_curr_act);
	regdump(-1);
	kernel_freeze();
}


static void kernel_exception_unknown(register_t excode) __dead2;
static void kernel_exception_unknown(register_t excode) {
	exception_printf(KRED"Unknown exception type '%lu' in  %s-%d"KRST"\n",
	       excode, kernel_acts[kernel_curr_act].name, kernel_curr_act);
	regdump(-1);
	kernel_freeze();
}


/*
 * Exception handler demux to various more specific exception
 * implementations.
 */
void kernel_exception(void) {
	static int entered = 0;
	entered++;
	KERNEL_TRACE("exception", "enters %d", entered);
	if(entered > 1) {
		KERNEL_ERROR("interrupt in interrupt");
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
		kernel_exception_syscall();
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
}
