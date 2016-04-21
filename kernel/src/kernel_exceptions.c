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
#include "lib.h"
#include "cp0.h"
#include "cp2.h"
#include "cheric.h"

/* We only create tasks for now, no delete */
struct mips_frame		kernel_exception_framep[MAX_PROCESSES];
struct cp2_frame		kernel_cp2_exception_framep[MAX_PROCESSES];
proc_t				kernel_procs[MAX_PROCESSES];
int 				kernel_curr_proc;
int 				kernel_next_proc;
struct mips_frame *		kernel_exception_framep_ptr;
struct cp2_frame *		kernel_cp2_exception_framep_ptr;

void kernel_proc_init(void) {
	KERNEL_TRACE("boot", "proc init");
	kernel_curr_proc = 0;
	kernel_next_proc = 1;
	kernel_procs[kernel_curr_proc].runnable = 1;
	kernel_exception_framep_ptr = &kernel_exception_framep[kernel_curr_proc];
	kernel_cp2_exception_framep_ptr = &kernel_cp2_exception_framep[kernel_curr_proc];
}

void kernel_reschedule(void) {
	kernel_assert(kernel_curr_proc < kernel_next_proc);
	size_t old_kernel_curr_proc = kernel_curr_proc;
	int tries = 0;
	again:
	if(++kernel_curr_proc == kernel_next_proc) {
		kernel_curr_proc = 0;
	}
	tries++;
	if(tries > MAX_PROCESSES) {
		KERNEL_ERROR("No process to schedule");
		kernel_freeze();
	}
	if(!kernel_procs[kernel_curr_proc].runnable) {
		//KERNEL_TRACE("exception", "proc '%ld' not runnable",kernel_curr_proc);
		goto again;
	}
	kernel_exception_framep_ptr = kernel_exception_framep + kernel_curr_proc;
	kernel_cp2_exception_framep_ptr = kernel_cp2_exception_framep + kernel_curr_proc;
	KERNEL_TRACE("exception", "Reschedule from task '%ld' to task '%ld'",
		old_kernel_curr_proc, kernel_curr_proc );
}

void kernel_proc_set_pc(register_t addr, size_t process) {
	kernel_exception_framep[process].mf_pc = addr;
	void * pcc = (void *) kernel_cp2_exception_framep_ptr->cf_pcc;
	pcc = __builtin_memcap_offset_set(pcc, addr);
	kernel_cp2_exception_framep[process].cf_pcc = pcc;
}

/* hack. Rewrite 'exec' instead. */
void tmp_exec_stuff(size_t pid, size_t base, size_t entry, size_t len) {
	kernel_cp2_exception_framep[pid].cf_pcc =
	 __builtin_memcap_perms_and(
	  __builtin_memcap_offset_set(
	   __builtin_memcap_bounds_set(
	    __builtin_memcap_offset_set(
	     kernel_cp2_exception_framep[pid].cf_pcc
	    , base )
	   , len)
	  , entry)
	 , 0b11110111);
	kernel_exception_framep[pid].mf_pc = entry;
	
	kernel_cp2_exception_framep[pid].cf_c0 =
	 __builtin_memcap_perms_and(
	  __builtin_memcap_bounds_set(
	   __builtin_memcap_offset_set(
	    kernel_cp2_exception_framep[pid].cf_c0
	   , base )
	  , len)
	 , 0b11111101);
	  
	kernel_exception_framep[pid].mf_a0 = pid;
	
	kernel_procs[pid].runnable = 1;
}

int task_create_bare(void) {
	/* return value (-1:fail, n=tak nb) */

	KERNEL_TRACE(__func__, "kcp:%d", kernel_curr_proc);

	/* zero everything */
	memset(kernel_exception_framep     + kernel_next_proc, 0, sizeof(struct mips_frame));
	memset(kernel_cp2_exception_framep + kernel_next_proc, 0, sizeof(struct cp2_frame ));
	
	/* set stack */
	size_t stack_size = 0x10000;
	void * stack = kernel_malloc(stack_size);
	if(!stack) {
		return -1;
	}
	kernel_cp2_exception_framep[kernel_next_proc].cf_c11 = stack;
	kernel_exception_framep[kernel_next_proc].mf_sp = stack_size;

	/* not runnable yet */	
	kernel_procs[kernel_next_proc].runnable = 0;
	
	/* set parent*/
	kernel_procs[kernel_next_proc].parent = kernel_curr_proc;
		
	/* do not set c0/c12/pc */
	
	/* done, update next_proc */
	kernel_next_proc++;
	return kernel_next_proc-1;
}

void kernel_skip_pid(int pid) {
	kernel_exception_framep[pid].mf_pc += 4; //assuming no branch delay slot
	void * pcc = (void *) kernel_cp2_exception_framep[pid].cf_pcc;
	pcc = __builtin_memcap_offset_increment(pcc, 4);
	kernel_cp2_exception_framep[pid].cf_pcc = pcc;
}

void kernel_skip(void) {
	kernel_exception_framep_ptr->mf_pc += 4; //assuming no branch delay slot
	void * pcc = (void *) kernel_cp2_exception_framep_ptr->cf_pcc;
	pcc = __builtin_memcap_offset_increment(pcc, 4);
	kernel_cp2_exception_framep_ptr->cf_pcc = pcc;
}



#if 0
/* Capsizefix cannot handle this nicely */
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
	"reserved",
	"reserved",
	"Access EPCC Violation",
	"Access KDC Violation",
	"Access KCC Violation",
	"Access KR1C Violation",
	"Access KR2C Violation",
	"reserved"
};
#endif

static void kernel_exception_capability(void)
{
	KERNEL_TRACE("exception", "kernel_capability");
	register_t capcause = __builtin_cheri_cause_get();
	int cause = (capcause >> 8) & 0x1F;
	
	if(cause == 5) { /* give them their own handler */
		kernel_ccall();
		return;
	}
	if(cause == 6) {
		kernel_creturn();
		return;
	}
	
	const char * capcausestr[0x20]; /* See above */
	capcausestr[0x00] = "None";
	capcausestr[0x01] = "Length Violation";
	capcausestr[0x02] = "Tag Violation";
	capcausestr[0x03] = "Seal Violation";
	capcausestr[0x04] = "Type Violation";
	capcausestr[0x05] = "Call Trap";
	capcausestr[0x06] = "Return Trap";
	capcausestr[0x07] = "Underflow of trusted system stack";
	capcausestr[0x08] = "User-defined Permission Violation";
	capcausestr[0x09] = "TLB prohibits store capability";
	capcausestr[0x0A] = "Requested bounds cannot be represented exactly";
	capcausestr[0x0B] = "reserved";
	capcausestr[0x0C] = "reserved";
	capcausestr[0x0D] = "reserved";
	capcausestr[0x0E] = "reserved";
	capcausestr[0x0F] = "reserved";
	capcausestr[0x10] = "Global Violation";
	capcausestr[0x11] = "Permit Execute Violation";
	capcausestr[0x12] = "Permit Load Violation";
	capcausestr[0x13] = "Permit Store Violation";
	capcausestr[0x14] = "Permit Load Capability Violation";
	capcausestr[0x15] = "Permit Store Capability Violation";
	capcausestr[0x16] = "Permit Store Local Capability Violation";
	capcausestr[0x17] = "Permit Seal Violation";
	capcausestr[0x18] = "reserved";
	capcausestr[0x19] = "reserved";
	capcausestr[0x1A] = "Access EPCC Violation";
	capcausestr[0x1B] = "Access KDC Violation";
	capcausestr[0x1C] = "Access KCC Violation";
	capcausestr[0x1D] = "Access KR1C Violation";
	capcausestr[0x1E] = "Access KR2C Violation";
	capcausestr[0x1F] = "reserved";
	
	int reg_num = capcause & 0xFF;
	kernel_printf(KRED "Capability exception catched! (0x%X: %s) [Reg C%d]" KRST"\n",
		cause, capcausestr[cause], reg_num);
	kernel_freeze();
}

static void kernel_exception_interrupt(void)
{
	register_t ipending = cp0_cause_ipending_get();
	if (ipending & MIPS_CP0_CAUSE_IP_TIMER) {
		kernel_timer();
	} else {
		KERNEL_ERROR("unknown interrupt");
	}
}


static void kernel_exception_data(register_t excode)
{
	KERNEL_ERROR("Data abort type '%d'", excode);
	kernel_freeze();
}


static void kernel_exception_unknown(register_t excode)
{
	KERNEL_ERROR("Unknown exception type '%d'", excode);
}

/*
 * Exception handler demux to various more specific exception
 * implementations.
 */
void
kernel_exception(void)
{
	static int entered = 0;
	KERNEL_TRACE("exception", "enters %d", entered++);
	
	#if 0
	static int n = 0;
	if(!((++n) & 0x7FFF)) {
		printf("En:%d\n", n);
	}
	#endif

	/*
	 * Check assumption that kernel is running at EXL=1.  The kernel is
	 * non-preemptive and will fail horribly if this isn't true.
	 */
	kernel_assert(cp0_status_exl_get() != 0);

	register_t excode = cp0_cause_excode_get();
	switch (excode) {
	case MIPS_CP0_EXCODE_INT:
		kernel_exception_interrupt();
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

	KERNEL_TRACE("exception", "returns %d", --entered);
	if(entered) {
		KERNEL_ERROR("interrupt in interrupt");
		kernel_freeze();
	}
}
