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

#include "asm_common.S"

# A little mis-named now. This is a bias for clc/csc used mostly for the context struct.
# It could be (1 << 11), but for now I'm not sure its needed.
.set INC_IM_MAX, 0

.macro TRACE_ON
    slti zero, zero, 0x1b
.endm

.macro TRACE_OFF
    slti zero, zero, 0x1e
.endm

.macro START_FUNC name
    .global \name
    ASM_VISIBILITY \name
    .type \name, "function"
    \name:
.endm

.macro END_FUNC name
    \name\()_end:
    .size \name, \name\()_end - \name
.endm

.macro cllc_rel dst, sym
    # This is the PC relative cllc, which only works with global bounds
    1: auipcc       \dst, %pcrel_hi(\sym)
    cincoffset      \dst, \dst, %pcrel_lo(1b)
.endm

.macro op_any_i12 rout, rin, constant, tmp, op
    .if (\constant >= -2048) && (\constant <= 2047)
        \op \rout, \rin, \constant
    .else
        li  \tmp, \constant
        \op \rout, \rin, \tmp
    .endif
.endm

.macro add_bi  rout, rin, constant, tmp
    op_any_i12 \rout, \rin, \constant, \tmp, add
.endm

.macro cinc_bi rout, rin, constant, tmp
    op_any_i12 \rout, \rin, \constant, \tmp, cincoffset
.endm

.macro subi     rout, rin, imm
    addi \rout, \rin, -\imm
.endm

.macro call_func_early sym
    # Manual offset calculation because this will be compiled with purecap but boot has global bounds
    cllc_rel        cra, \sym
    cjalr           cra, cra
.endm

.macro call_func sym
    lui         ra, %captab_call_hi(\sym)
    cincoffset  cra, cgp, ra
    clc         cra, %captab_call_lo(\sym)(cra)
    cjalr       cra, cra
.endm

# la is banned because purecap and/or relative.
# But we are using la in conjunction with capability adds in init, so limited use is fair

.macro la_relative reg sym
    lui \reg, %hi(\sym)
    addi \reg, \reg, %lo(\sym)
.endm

# same as la but assumes the 32-bit (signed) symbol will sign extend, and removes this extention.
.macro lau_relative reg, sym
    la_relative \reg, \sym + 0xFFFFFFFF00000000ULL
    slli \reg, \reg, 32
    srli \reg, \reg, 32
.endm

# 32-bit version. NOTE: Sadly, this instruction does not seem implemented yet.
.macro cclear_all mask
    .if (\mask & 0xFF)
        cclear 0, (\mask & 0xFF)
    .endif
    .if ((\mask >> 8) & 0xFF)
        cclear 1, ((\mask >> 8) & 0xFF)
    .endif
    .if ((\mask >> 16) & 0xFF)
        cclear 2, ((\mask >> 16) & 0xFF)
    .endif
    .if ((\mask >> 24) & 0xFF)
        cclear 3, ((\mask >> 24) & 0xFF)
    .endif
.endm

/* This would work, if only cclear was implemented
#include "assembly_utils.h"

# and again, but macro-fied to use abi names
#define cclear_regs(...) cclear_all (Reg_Encode_All(__VA_ARGS__))
*/

.macro cclear_all_with_moves regs:vararg
    .irp    reg, \regs
        move    \reg, zero
    .endr
.endm

#define cclear_regs(...) cclear_all_with_moves __VA_ARGS__
