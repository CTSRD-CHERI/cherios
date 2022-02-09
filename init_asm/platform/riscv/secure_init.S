/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Lawrence Esswood
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
#include "cprogram.h"
#include "elf.h"

START_HIDDEN_FUNC start

# TODO: We have to set up most of the registers expected by start_common
# reg_abi has which registers are which
# Things we need to construct:

# Stack / Unsafe stack
# Segment table

# tls_seg_offset
# tls_seg_size
# tls_prototype
# tls_prototype_size
# code_segment_offset
# code_write_cap
# data_seg_offset

# We also have a few extra arguments from foundation_enter, which mostly just need saving
# ca5: foundation_data cap
# ca6/idc: likely null, but would be the unlocked idc
# ca7: our auth key for the foundation

// Get a symbol in the foundation early on
.macro get_sym_f_help dst, sym, dst_as_x
cllc_rel        \dst, \sym
csetaddr        \dst, ca5, \dst_as_x
.endm
#define GET_SYM_F(dst, sym) get_sym_f_help dst, sym, Reg_Cap_To_Int(dst)

    # Protect against double entry
    GET_SYM_F       (cs0, entry_lock)
    li              s1, 1
    camomaxu.w      s1, s1, (cs0)
    bnez            s1, double_entry_trap

    # Build a stack
    GET_SYM_F       (csp, secure_stack)
    csetbounds_bi   csp, csp, DEFAULT_STACK_SIZE, s0
    cinc_bi         csp, csp, DEFAULT_STACK_SIZE, s0

    # Construct a segment table and offset table (we steal a bit from our stack)
    cincoffset      csp, csp, -(MAX_SEGS * (CAP_SIZE + REG_SIZE))
    csetbounds      abi_seg_tbl, csp, MAX_SEGS * (CAP_SIZE + REG_SIZE)

    # Move cert into correct register
    # (FIXME: This register seems to be playing dual purpose as both the certficate register for calling foundation_enter,
    # and the auth_key passed to start_cmmon)
    cmove           abi_cert, ca7

# Some registers that are spare for temps
# (cannot be ones used in reg_abi.h, apart from those we have already consumed to call foundation_enter):
# x5-7 (t0-2), x1 (ra), x22 (s6), x30 (t5)

#define tmp0    t0
#define ctmp0   ct0
#define tmp1    t1
#define ctmp1   ct1
#define tmp2    t2
#define ctmp2   ct2
#define tmp3    ra
#define ctmp3   cra
#define tmp4    s6
#define ctmp4   cs6
#define tmp5    t5
#define ctmp5   ct5
// We dont use abi_code_write until after we are finished with max
#define hlp(X)  Reg_Cap_To_Int(X)
#define max     hlp(abi_code_write)

    # Now parse the ELF
    clh             tmp0, EHDR_OFF_e_phnum(ca5) # e_phnum
    cld             tmp1, EHDR_OFF_e_phoff(ca5) # e_phoff

    cincoffset      ctmp1, ca5, tmp1            # cap to phdrs[0]
    li              tmp2, 0                     # offset to segtbl
    slli            tmp0, tmp0, CAP_SIZE_BITS   # loop until tmp0 is tmp2
    # Iterate over all PHDRs

loop_start:
    addi            tmp2, tmp2, CAP_SIZE        # offset for seg tbl
    clw             tmp3, PHDR_OFF_p_type(ctmp1)    # P_TYPE
    cld             tmp4, PHDR_OFF_p_vaddr(ctmp1)   # VADDR
    cld             tmp5, PHDR_OFF_p_memsz(ctmp1)   # memsz
    addi            tmp3, tmp3, -PT_LOAD
    bnez            tmp3, is_not_pt_load

is_pt_load:
    # maybe update max addr
    add             tmp3, tmp4, tmp5                # segment vaddr end
    bleu            tmp3, max, 1f
    move            max, tmp3
    1:

    # Make cap to segment
    cincoffset      ctmp3, ca5, tmp4
    csetbounds      ctmp3, ctmp3, tmp5

    # Store cap in table
    cincoffset      ctmp5, abi_seg_tbl, tmp2
    csc             ctmp3, 0(ctmp5)
    # Store vaddr in table
    srl             tmp5, tmp2, (CAP_SIZE_BITS - REG_SIZE_BITS)
    cincoffset      ctmp5, abi_seg_tbl, tmp5
    csd             tmp4, (MAX_SEGS * CAP_SIZE)(ctmp5)
    # Assume W is data, otherwise code
    clw             tmp5, PHDR_OFF_p_flags(ctmp1)
    andi            tmp5, tmp5, PF_W
    bnez            tmp5, is_w
is_e:
    move            abi_code_seg_offset, tmp2
    j               loop_footer
is_w:
    move            abi_data_seg_offset, tmp2
    j               loop_footer

is_not_pt_load:
    addi            tmp3, tmp3, (PT_LOAD-PT_TLS)
    bnez            tmp3, loop_footer
is_tls:
    cld             abi_tls_fil_size,  PHDR_OFF_p_filesz(ctmp1)
    move            abi_tls_seg_offset, tmp2
    move            abi_tls_mem_size, tmp5
    cincoffset      abi_tls_proto, ca5, tmp4
    csetbounds      abi_tls_proto, abi_tls_proto, tmp5
    # Store vaddr in table
    srl             tmp5, tmp2, (CAP_SIZE_BITS - REG_SIZE_BITS)
    cincoffset      ctmp5, abi_seg_tbl, tmp5
    csd             tmp4, (MAX_SEGS * CAP_SIZE)(ctmp5)

loop_footer:
    cincoffset     ctmp1, ctmp1, PHDR_SIZE     # increment phdr ptr
    bne            tmp0, tmp2, loop_start
loop_end:

# We put the TLS segment for the first thread after every other segment (thus were keeping track of the max)
# Round up to a cap first

    addi            max, max, CAP_SIZE-1
    andi            max, max, ~(CAP_SIZE-1)
# Then make the cap and store it in the seg_tbl
    cincoffset      ctmp0, ca5, max
    csetbounds      ctmp0, ctmp0, abi_tls_mem_size
    cincoffset      ctmp1, abi_seg_tbl, abi_tls_seg_offset
    csc             ctmp0, 0(ctmp1)

# Make an executable version of the code cap in the segtbl, and swap it
    cincoffset      ctmp0, abi_seg_tbl, abi_code_seg_offset
    clc             abi_code_write, 0(ctmp0)
    auipcc          ctmp1, 0
    csetaddr        ctmp1, ctmp1, hlp(abi_code_write)
    cgetlen         tmp2, abi_code_write
    csetbounds      ctmp1, ctmp1, tmp2
    csc             ctmp1, 0(ctmp0)

# Make a null safe stack
    cmove           abi_unsafe, cnull

call_func_early start_common

double_entry_trap:
    TRACE_ON
    trap
END_FUNC start

.bss
# A stack in our BSS we can trust
MAKE_SYM secure_stack, DEFAULT_STACK_SIZE, DEFAULT_STACK_ALIGN_p2
# A lock for whether the first thread as entered
MAKE_SYM entry_lock, 4, 2