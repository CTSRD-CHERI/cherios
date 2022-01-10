/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lawrence Esswood
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include "nano/nanokernel.h"
#include "nano/nanotypes.h"
#include "kernel_exceptions.h"
#include "activations.h"
#include "klib.h"
#include "cpu.h"

DEFINE_ENUM_CASE(riscv_cause, RISCV_CAUSE_LIST)

DEFINE_ENUM_AR(cap_cause_exception_t, CAP_CAUSE_LIST)

void handle_exception_kill(act_t* kernel_curr_act, exection_cause_t* ex_info) {

    kernel_printf("Exception %s killing activation %s\n", enum_riscv_cause_tostring(ex_info->cause & RISCV_CAUSE_MASK), kernel_curr_act->name);
    int regnum = -1;
    if ((ex_info->cause & RISCV_CAUSE_MASK) == RISCV_CAUSE_CHERI) {
        cap_exception_t exception = parse_cause(ex_info->stval);
        kernel_printf("Cheri Exception: %s. Regnum: %d\n", enum_cap_cause_exception_t_tostring(exception.cause), exception.reg_num);
        regnum = exception.reg_num;
    }
    regdump(regnum, kernel_curr_act);
    kernel_freeze();
}

void kernel_exception(__unused context_t swap_to, context_t own_context) {

    exection_cause_t ex_info;

    uint8_t cpu_id = cpu_get_cpuid();

    set_exception_handler(own_context, cpu_id);

    kernel_interrupts_init(1, cpu_id);
    act_t* kernel_curr_act;

    while (1) {
        // Get a new activation to switch to
        kernel_curr_act = sched_get_current_act_in_pool(cpu_id);
        context_switch(ACT_ARG_LIST(kernel_curr_act), kernel_curr_act->context);
        // Get which activation should have taken the exception
        kernel_curr_act = sched_get_current_act_in_pool(cpu_id);
        // TODO RISCV handle exceptions
        get_last_exception(&ex_info);
        kernel_assert(ex_info.victim_context == kernel_curr_act->context);

        if (ex_info.cause >> RISCV_CAUSE_INT_SHIFT) {
            // Interrupt
            kernel_printf("Kernel interrupt: TODO\n");
        } else {
            // Synchronous exception
            kernel_printf("Synchronous exception. Code : %lx\n", ex_info.cause);
            switch(ex_info.cause) {
                default:
                    handle_exception_kill(kernel_curr_act, &ex_info);
            }

        }

    }
}
