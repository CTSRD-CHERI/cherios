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
#include <nano/nanotypes.h>
#include "activations.h"
#include "klib.h"
#include "cp0.h"
#include "kernel_exceptions.h"

/*
 * Exception demux
 */

DEFINE_ENUM_AR(cap_cause_exception_t, CAP_CAUSE_LIST)

static void kernel_exception_capability(register_t ccause, act_t* kernel_curr_act) __dead2;
static void kernel_exception_capability(register_t ccause, act_t* kernel_curr_act) {
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

	regdump(exception.reg_num, kernel_curr_act);
	kernel_freeze();
}

static void kernel_exception_data(register_t excode, act_t* kernel_curr_act, size_t badvaddr) __dead2;
static void kernel_exception_data(register_t excode, act_t* kernel_curr_act, size_t badvaddr) {
	exception_printf(KRED"Data abort type %ld, BadVAddr:0x%lx in %s"KRST"\n",
	       excode, badvaddr,
	       kernel_curr_act->name);
	regdump(-1, kernel_curr_act);
	kernel_freeze();
}

static void kernel_exception_trap(act_t* kernel_curr_act) __dead2;
static void kernel_exception_trap(act_t* kernel_curr_act) {
	exception_printf(KRED"trap in %s"KRST"\n"
					 , kernel_curr_act->name);
	regdump(-1, kernel_curr_act);
	kernel_freeze();
}

static void kernel_exception_unknown(register_t excode, act_t* kernel_curr_act) __dead2;
static void kernel_exception_unknown(register_t excode, act_t* kernel_curr_act) {
	exception_printf(KRED"Unknown exception type '%ld' in  %s"KRST"\n",
	       excode, kernel_curr_act->name);
	regdump(-1, kernel_curr_act);
	kernel_freeze();
}


static void kernel_exception_tlb(register_t badvaddr, act_t* kernel_curr_act);
static void kernel_exception_tlb(register_t badvaddr, act_t* kernel_curr_act) {
	if(memgt_ref == NULL) {
		exception_printf(KRED"Virtual memory exception (%lx) before memmgt created\n"KRST, badvaddr);
        regdump(-1, kernel_curr_act);
        kernel_freeze();
	}
	if(kernel_curr_act == memgt_ref) {
		exception_printf(KRED"Virtual memory exception in memmgt is not allowed\n"KRST);
		regdump(-1, kernel_curr_act);
		kernel_freeze();
	}
    if(kernel_curr_act->is_idle) {
        regdump(-1, kernel_curr_act);
    }

    kernel_assert(!kernel_curr_act->is_idle);
    // Order is important here. We need to send the message first to unblock memgt.
    // This can however result in the commit coming in before the block. sched handles this for us.

    kernel_curr_act->last_vaddr_fault = badvaddr;
    kernel_curr_act->commit_early_notify = 0;
    if(msg_push(act_create_sealed_ref(kernel_curr_act), kernel_curr_act->name, NULL, NULL, badvaddr, 0, 0, 0, 2, memgt_ref, kernel_curr_act, NULL))
        kernel_printf(KRED"Could not send commit event. Queue full\n"KRST);
    sched_block_until_event(kernel_curr_act, memgt_ref, sched_wait_commit, 0, 1);

    kernel_curr_act->last_vaddr_fault = badvaddr;
}

extern int in_bench;
/*
 * Exception handler demux to various more specific exception
 * implementations.
 */
static void handle_exception_loop(context_t* own_context_ptr) {
    context_t own_context = *own_context_ptr;
    exection_cause_t ex_info;

    uint8_t cpu_id = cp0_get_cpuid();


    // We fire a dummy exception for starting CPUs to give their exception context a chance
    kernel_interrupts_init(1, cpu_id);

    act_t* kernel_curr_act = sched_get_current_act_in_pool(cpu_id)->context;

    context_switch(ACT_ARG_LIST_NULL, kernel_curr_act);


    while(1) {

        get_last_exception(&ex_info);

        if(in_bench == 0) {

        }
        kernel_assert(in_bench == 0);
        kernel_curr_act = sched_get_current_act_in_pool(cpu_id);

        if(ex_info.victim_context != own_context) {
            // We only do this as handles are not guaranteed to stay fresh (although they are currently)
            kernel_curr_act->context = ex_info.victim_context;
        } else {
            // This happens if an interrupt happened during the exception level. As soon as we exit
            // Another exception happens and so take another.
        }

        register_t excode = cp0_cause_excode_get(ex_info.cause);

        KERNEL_TRACE("exception", "in %s. Code %lx. CPU_ID: %u",
                     kernel_curr_act->name,
                     (unsigned long)(ex_info.cause),
                    cpu_id);

        // If we have interrupts to handle do this first. Then we can handle any other excodes
        if((cp0_status_get() & ex_info.cause & 0xFF00)) kernel_interrupt(ex_info.cause, cpu_id);

        switch (excode) {
            case MIPS_CP0_EXCODE_INT:
                break;

            case MIPS_CP0_EXCODE_SYSCALL:
                exception_printf(KRED"Synchronous syscalls now use the ccall interface"KRST"\n");
                regdump(-1, kernel_curr_act);
                kernel_freeze();
                break;

            case MIPS_CP0_EXCODE_C2E:
                kernel_exception_capability(ex_info.ccause, kernel_curr_act);
                break;
            case MIPS_CP0_EXCODE_TLBL:
            case MIPS_CP0_EXCODE_TLBS:
                kernel_exception_tlb(ex_info.badvaddr, kernel_curr_act);
                break;
            case MIPS_CP0_EXCODE_ADEL:
            case MIPS_CP0_EXCODE_ADES:
            case MIPS_CP0_EXCODE_IBE:
            case MIPS_CP0_EXCODE_DBE:
                kernel_exception_data(excode, kernel_curr_act, ex_info.badvaddr);
                break;

            case MIPS_CP0_EXCODE_TRAP:
                kernel_exception_trap(kernel_curr_act);
                break;
            default:
                kernel_exception_unknown(excode, kernel_curr_act);
                break;
        }

        // We may have changed context due to this exception
        kernel_curr_act = sched_get_current_act_in_pool(cp0_get_cpuid());

        KERNEL_TRACE("exception", "restoring %s", kernel_curr_act->name);
        context_switch(ACT_ARG_LIST(kernel_curr_act), kernel_curr_act->context);
    }
}

#define EXCEPTION_STACK_SIZE 0x4000
typedef struct {
    capability stack [EXCEPTION_STACK_SIZE/sizeof(capability)];
}exception_stack_t;

static exception_stack_t exception_stacks[SMP_CORES-1]; // Core 0's stack is given by the call of kernel_exception
static context_t contexts[SMP_CORES];

static context_t make_exception_context(uint8_t cpu_id) {
    reg_frame_t frame;
    bzero(&frame, sizeof(reg_frame_t));

    // Program counter
    frame.cf_pcc = frame.cf_c12 = &handle_exception_loop;

    // DDC
    frame.cf_default = cheri_getdefault();

    // Stack
    frame.cf_c11 =
        cheri_setoffset(cheri_setbounds(&exception_stacks[cpu_id-1], EXCEPTION_STACK_SIZE), EXCEPTION_STACK_SIZE);

    // Unsafe stack
    frame.cf_c10 = NULL;

    // Globals / locals (locals probably null)
    frame.cf_c25 = cheri_getreg(25);
    frame.cf_idc = cheri_getreg(26);

    frame.cf_c3 = &(contexts[cpu_id]);

    return create_context(&frame, NULL);
}

void kernel_exception(__unused context_t swap_to, context_t own_context) {

    // Create an exception context for every core. The current context is re-used for core0
    for(uint8_t cpu_id = 0; cpu_id != SMP_CORES; cpu_id++ ) {
        context_t ex_context = cpu_id == 0 ? own_context : make_exception_context(cpu_id);
        contexts[cpu_id] = ex_context;
        set_exception_handler(ex_context, cpu_id);
    }

    handle_exception_loop(&contexts[0]);
}
