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

.macro START_HIDDEN_FUNC name
    .global \name
    .hidden \name
    .type \name, "function"
    \name:
.endm

.macro END_FUNC name
    \name\()_end:
    .size \name, \name\()_end - \name
.endm

.macro MAKE_SYM name size align
    .p2align \align
    .global \name
    .type \name, "object"
    \name:
    .space  \size
    .size \name, \size
.endm

.macro cllc_rel dst, sym
    # This is the PC relative cllc, which only works with global bounds
    1: auipcc       \dst, %pcrel_hi(\sym)
    cincoffset      \dst, \dst, %pcrel_lo(1b)
.endm

.macro op_any_i5 rout, rin, constant, tmp, op
    .if (\constant >= 0) && (\constant <= 31)
        \op\()i \rout, \rin, \constant
    .else
        li \tmp, \constant
        \op \rout, \rin, \tmp
    .endif
.endm

.macro csrrs_bi rout, rin, constant, tmp
    op_any_i5 \rout, \rin, \constant, \tmp, csrrs
.endm

.macro csrrc_bi rout, rin, constant, tmp
    op_any_i5 \rout, \rin, \constant, \tmp, csrrc
.endm

.macro op_any_i12 rout, rin, constant, tmp, op, linear
    .if (\constant >= -2048) && (\constant <= 2047)
        # use op once
        \op \rout, \rin, \constant
    .elseif (\constant >= -4096) && (\constant <= 4094) && \linear
        # use op twice
        .if (\constant < 0)
            \op \rout, \rin, -2048
            \op \rout, \rout, (\constant + 2048)
        .else
            \op \rout, \rin, 2047
            \op \rout, \rout, (\constant - 2047)
        .endif
    .else
        # use lui + add to load a constant
        li  \tmp, \constant
        \op \rout, \rin, \tmp
    .endif
.endm

.macro op_any_u12 rout, rin, constant, tmp, op
    .if (\constant >= 0) && (\constant < 4096)
        \op \rout, \rin, \constant
    .else
        li \tmp, \constant
        \op \rout, \rin, \tmp
    .endif
.endm

.macro csetbounds_bi rout, rin, constant, tmp
    op_any_u12 \rout, \rin, \constant, \tmp, csetbounds
.endm

.macro add_bi  rout, rin, constant, tmp
    op_any_i12 \rout, \rin, \constant, \tmp, add, 1
.endm

.macro cinc_bi rout, rin, constant, tmp
    op_any_i12 \rout, \rin, \constant, \tmp, cincoffset, 1
.endm

.macro and_bi rout, rin, constant, tmp
    op_any_i12 \rout, \rin, \constant, \tmp, and, 0
.endm

.macro subi     rout, rin, imm
    addi \rout, \rin, -\imm
.endm

.macro call_func_early sym
    # Manual offset calculation because this will be compiled with purecap but boot has global bounds
    cllc_rel        cra, \sym
    cjalr           cra, cra
.endm

.macro load_func_reg dst, sym, tmp
    .type   \sym, "function"
    lui         \tmp, %captab_call_hi(\sym)
    cincoffset  \dst, cgp, \tmp
    clc         \dst, %captab_call_lo(\sym)(\dst)
.endm

.macro load_func sym
    load_func_reg   cra, \sym, ra
.endm

.macro call_func_loaded
    cjalr       cra, cra
.endm

.macro call_func sym
    load_func   \sym
    call_func_loaded
.endm

.macro jump_sym sym
    load_func_reg   ct0, \sym, t0
    cjr             ct0
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

#include "assembly_utils.h"

/* This would work, if only cclear was implemented

# and again, but macro-fied to use abi names
#define cclear_regs(...) cclear_all (Reg_Encode_All(__VA_ARGS__))
*/

.macro cclear_all_with_moves regs:vararg
    .irp    reg, \regs
        move    \reg, zero
    .endr
.endm

#define cclear_regs(...) cclear_all_with_moves __VA_ARGS__

.macro get_sym_help dst, sym, tmp, tls, base_reg
    lui         \tmp, %captab\tls\()_hi(\sym)
    cincoffset  \dst, \base_reg, \tmp
    clc         \dst, %captab\tls\()_lo(\sym)(\dst)
.endm

#define GET_SYM(dst, sym) get_sym_help dst, sym, Reg_Cap_To_Int(dst),,cgp
#define GET_TLS_SYM(dst, sym) .type sym, "tls_object"; get_sym_help dst, sym, Reg_Cap_To_Int(dst),_tls,abi_local

.macro get_sym_value_help dst, xtmp, ctmp, sym, tls, dst_size, base_reg
    .if \dst_size == 8
        get_sym_help \ctmp, \sym, \dst, \tls, \base_reg
        cld \dst, 0(\ctmp)
    .else
        get_sym_help \dst, \sym, \xtmp, \tls, \base_reg
        clc \dst, 0(\dst)
    .endif
.endm

#define GET_SYM_VAL(dst, sym) get_sym_value_help dst, Reg_Cap_To_Int(dst), Reg_Int_To_Cap(dst), sym,,Reg_Size(dst), cgp
#define GET_TLS_SYM_VAL(dst, sym) get_sym_value_help dst, Reg_Cap_To_Int(dst), Reg_Int_To_Cap(dst), sym,_tls,Reg_Size(dst), abi_local

.macro spill_stack_help op4, op8, reg, size, tail:vararg
    .set spill_ctr, spill_ctr + ((-spill_ctr) % \size)
    .set spill_ctr, (spill_ctr + \size)
    .set \reg\()_spill_offset, spill_ctr
    .if \size == 8
        \op4 \reg, -(spill_ctr)(csp)
    .else
        \op8 \reg, -(spill_ctr)(csp)
    .endif

    .ifnb  \tail
        spill_stack_help \op4, \op8, \tail
    .endif
.endm

.macro spill_stack dummy, regs:vararg
    .set spill_ctr, 0
    spill_stack_help csd, csc, \regs
    # Align stack
    .set spill_ctr, spill_ctr + ((-spill_ctr) % 16)
    cincoffset  csp, csp, -(spill_ctr)
.endm

.macro load_spill dst, reg, size
    .if \size == 8
        cld \dst, (spill_ctr-\reg\()_spill_offset)(csp)
    .else
        clc \dst, (spill_ctr-\reg\()_spill_offset)(csp)
    .endif
.endm

#define SPILL_STACK(...) spill_stack dummy AttachSizes(__VA_ARGS__)
#define LOAD_SPILL(Dst, X) load_spill Dst, X, Reg_Size(X)

.macro restore_stack dummy, regs:vararg
    cincoffset csp, csp, (spill_ctr)
    .set spill_ctr, 0
    spill_stack_help cld, clc, \regs
.endm

#define RESTORE_STACK(...) restore_stack dummy AttachSizes(__VA_ARGS__)

.macro swap a, b, tmp
    move \tmp, \a
    move \a, \b
    move \b, \tmp
.endm

.macro TODO_SPIN
    TRACE_ON
    1: j 1b
.endm

.macro trap
    .word   0
.endm

