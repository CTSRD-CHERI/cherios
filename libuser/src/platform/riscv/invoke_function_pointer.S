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

START_FUNC call_function_pointer_arg_mem

clc             ct2, CTLP_OFFSET_CDS(abi_local)
load_func_reg   ct3, plt_common_complete_trusting, t3
beqz            t2, mode_trusting               # Use complete trust if cds is NULL
load_func_reg   ct3, plt_common_untrusting, t3  # otherwise use untrusting
mode_trusting:
.weak invoke_c1
.weak invoke_c2
GET_TLS_SYM     (PLT_REG_TARGET, invoke_c1)
GET_TLS_SYM     (PLT_REG_TARGET_DATA, invoke_c2)

# Try to shortcircuit
CSetEqualExact  t4, PLT_REG_TARGET, abi_local
beqz            t4, go_via_helper
# We are making an in domain call, might still have to unseal
cgetsealed      t3, PLT_REG_TARGET
beqz            t3, handle_trusted
.set offset_trusted, (3 * 4)
.set offset_untrusted, (5 * 4)
cunseal         PLT_REG_TARGET, PLT_REG_TARGET, ct2
cincoffset      PLT_REG_TARGET, PLT_REG_TARGET, offset_untrusted - offset_trusted
handle_trusted:
cincoffset      ct3, PLT_REG_TARGET, offset_trusted
go_via_helper:
cjr             ct3

END_FUNC call_function_pointer_arg_mem
