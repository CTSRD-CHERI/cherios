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
#include "cheric.h"
#include "reg_abi.h"

# TODO RISCV This might need further setup
# TODO RISCV This needs to park cores (harts?) other than zero for smp.

.text

START_FUNC  start

# CheriOS enforces W^X. Take execute out of DDC.
cspecialr   ct1, ddc
li          t2, CHERI_PERM_ALL & ~(CHERI_PERM_EXECUTE)
candperm    ct1, ct1, t2
cspecialw   ddc, ct1

# Actually auipc because we are not yet in cap mode!
1: auipcc       ca0, %pcrel_hi(remove_write_label)
cincoffset      ca0, ca0, %pcrel_lo(1b)
# So use cspecialr to get a capability
cspecialr       ct3, pcc
csetaddr        ca0, ct3, a0
# Take stores out of PCC
li             t3, CHERI_PERM_ALL & ~(CHERI_PERM_STORE | CHERI_PERM_STORE_CAP)
candperm       ca0, ca0, t3
# Also turn on cap mode
li              t3, 1
csetflags       ca0, ca0, t3
# Have to be explicit about the cap, because the assembler thinks we are in cap mode. We are not.
jr.cap          ca0

remove_write_label:

# Set up boot stack allocated by the linker
lau_relative t3, __start_boot_stack
la_relative  t4, __size_boot_stack

csetaddr        csp, ct1, t3
csetboundsexact csp, csp, t4
cincoffset      csp, csp, t4

# Init globals.
call_func_early crt_init_globals_boot
cmove           abi_local, ca0

# Zero BSS
cspecialr       ca0, ddc
call_func       crt_init_bss

# And finally go to main
cspecialr       ca0, pcc
csetaddr        ca0, ca0, zero
call_func       bootloader_main

END_FUNC    start
