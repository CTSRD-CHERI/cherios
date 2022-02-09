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

#include "asm.S"
#include "dylink.h"

START_FUNC __cross_domain_kernel_message_send

#if (KERNEL_FASTPATH)
TODO_SPIN
#else

# TODO RISCV
// Eventually we want a fastpath here. For now, we just default to a slow path.
/* Kernel message send has the same signature, apart from the return type. _ret returns a reference to the actual return values
__used ret_t* kernel_message_send_ret(capability c3, capability c4, capability c5, capability c6,
					 register_t a0, register_t a1, register_t a2, register_t a3,
					 act_t* target_activation, ccall_selector_t selector, register_t v0)
*/
// This needs doing better, maybe inside stubs.S based on the length of CSP?
// We need to get the stack past arguments off the stack before the swtich.
// We put them in temps we know are not used in stubs.S
clc             ct3, 0(csp)
clc             ct4, CAP_SIZE(csp)
// The normal cross domain sequence
clc             ct1, CTLP_OFFSET_CDL(abi_local)
cjalr           ct1, ct1
// TODO: when stack passing is fixed, this will change. For now, we have to quickly transfer stack passed arguments
// Before we clobber csp, we have to grab stack passed arguments
clc             csp, CTLP_OFFSET_CSP(abi_local)
clc             cgp, CTLP_OFFSET_CGP(abi_local)
// Push a stack frame because we have to unpack the ret struct
SPILL_STACK     (cra, ct4, ct3)
// Push stack passed arguments

call_func       kernel_message_send_ret
// unpack return
clc             ca1, CAP_SIZE(ca0)
clc             ca0, 0(ca0)
RESTORE_STACK   (cra)
cret
#endif
END_FUNC __cross_domain_kernel_message_send

START_FUNC __cross_domain_kernel_fastpath_wait
#if (KERNEL_FASTPATH)
// TODO
TODO_SPIN
#else
clc             cgp, (CTLP_OFFSET_CGP)(abi_local)
jump_sym        __cross_domain_fastpath_bailout
#endif
END_FUNC __cross_domain_kernel_fastpath_wait

