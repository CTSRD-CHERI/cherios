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

#include "klib.h"
#include "kernel_exceptions.h"
#include "marshall_args.h"

void kernel_exception_tlb(register_t badvaddr, act_t* kernel_curr_act) {
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

#if (K_DEBUG)
    kernel_curr_act->commit_faults++;
#endif

    kernel_curr_act->last_vaddr_fault = badvaddr;
    kernel_curr_act->commit_early_notify = 0;
    if(msg_push(MARSHALL_ARGUMENTS(act_create_sealed_ref(kernel_curr_act), kernel_curr_act->name, badvaddr), 2, memgt_ref, kernel_curr_act, NULL))
        kernel_printf(KRED"Could not send commit event. Queue full\n"KRST);
    sched_block_until_event(kernel_curr_act, memgt_ref, sched_wait_commit, 0, 1);

    kernel_curr_act->last_vaddr_fault = badvaddr;
}
