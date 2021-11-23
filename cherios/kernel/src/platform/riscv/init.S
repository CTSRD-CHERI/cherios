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
#include "reg_abi.h"

# TODO RISCV

.text

START_FUNC start

# Save everything we have been passed by the nanokernel

move            s0, a0      # Init starts at
move            s1, a1      # init entry
move            s2, a2      # init tls base
cmove           cs3, ca4    # nano if
cmove           cs4, ca5    # nano data
cmove           cs5, ca6    # context handle
cmove           cs6, ca7    # request token

# Create stack
la_relative     t3, __start_exception_stack
la_relative     t4, __stop_exception_stack
cspecialr       csp, ddc
cincoffset      csp, csp, t3
csetboundsexact csp, csp, t4
cincoffset      csp, csp, t4

# Init globals.
call_func_early  crt_init_globals_kernel
cmove           abi_local, ca0

# Zero BSS.
call_func       crt_init_bss

cmove           ca0, cs3
cmove           ca1, cs4
cmove           ca2, cs5
auipcc          ca3, 0
cmove           ca4, cs6
move            a5, s0
move            a6, s1
move            a7, s2
call_func       cherios_main
/*
int cherios_main(nano_kernel_if_t* interface,
				 capability def_data,
				 context_t own_context,
                 capability global_pcc,
                 if_req_auth_t req_auth,
				 size_t init_base,
                 size_t init_entry,
                 size_t init_tls_base)

*/

END_FUNC start
