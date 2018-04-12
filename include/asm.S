#-
# Copyright (c) 2011 Robert N. M. Watson
# Copyright (c) 2016 Hadrien Barral
# Copyright (c) 2017 Lawrence Esswood
# All rights reserved.
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
# ("CTSRD"), as part of the DARPA CRASH research programme.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#define __ASSEMBLY__ 1

.set noreorder
.set nobopt
.set noat

.set CAP_SIZE, (MIPS_SZCAP/8)
.set REG_SIZE, (64/8)
.set MIPS_FRAME_SIZE, (32*REG_SIZE)
.set REG_SIZE_BITS, 3
.set CHERI_CAP_FRAME_SIZE, (28 * CAP_SIZE)
.set CHERI_FRAME_SIZE, (MIPS_FRAME_SIZE + CHERI_CAP_FRAME_SIZE)
.set FRAME_C3_OFFSET, MIPS_FRAME_SIZE + (3 * CAP_SIZE)
.set FRAME_a0_OFFSET, 3 * REG_RIZE

.set INC_IM_MAX, 0x3E0

.ifeq MIPS_SZCAP-256
.set CAP_SIZE_BITS, 5
.set U_PERM_BITS, 16
.else
.ifeq MIPS_SZCAP-128
.set CAP_SIZE_BITS, 4
.set U_PERM_BITS, 4
.else
.error "Unknown cap size"
.endif
.endif

.macro TRACE_ON
li  $zero, 0xbeef
.endm

.macro TRACE_OFF
li  $zero, 0xdead
.endm

.set Perm_Global, 1 << 0
.set Perm_Execute, 1 << 1
.set Perm_Load, 1 << 2
.set Perm_Store, 1 << 3
.set Perm_Load_Capability, 1 << 4
.set Perm_Store_Capability, 1 << 5
.set Perm_Store_Local_Capability, 1 << 6
.set Perm_Seal, 1 << 7
.set Perm_Access_System_Registers, 1 << 10
.set Perm_All, (1 << (11 + U_PERM_BITS)) - 1

#TODO this will not work until csc/cld supports fixups

#.macro local_var name, size, align
#    .align \align
#    \name\()_label : .space \size
#    .set \name, 0 #\name\()_label - locals_start
#.endm

# Depracated

#.macro dla_tls reg, sym
#    lui     \reg, %tprel_hi(\sym)
#    daddiu  \reg, \reg, %tprel_lo(\sym)
#.endm


.macro get_sym reg, sym
   clcbi	\reg, %captab20(\sym)($c25)
.endm

.macro get_tls_sym reg, sym
    clcbi	\reg, %captab_tls20(\sym)($c26)
.endm

.macro get_sym_val_array reg, sym, size, tmp_cap, base, index
    get_sym \tmp_cap, \sym
    cl\size \reg, \base, \index(\tmp_cap)
.endm

.macro get_sym_val reg, sym, size, tmp_cap
    get_sym_val_array \reg, \sym, \size, \tmp_cap, $zero, 0
.endm

.macro get_tls_sym_val reg, sym, size, tmp_cap
    get_tls_sym \tmp_cap, \sym
    cl\size \reg, $zero, 0(\tmp_cap)
.endm

.macro get_tls_sym_cap reg, sym
    get_tls_sym_val \reg, \sym, c, \reg
.endm

.macro call_func f
    clcbi   $c12, %capcall20(\f)($c25)
    cjalr   $c12, $c17
    cmove   $c18, $idc
.endm

.macro call_func_reg
    cjalr   $c12, $c17
    cmove   $c18, $idc
.endm

.macro ERET_BUGLESS
    cgetoffset $k1, $epcc                                # counters an eret bug
    dmtc0      $k1, $14
    eret
    nop
.endm

.macro START_LOCALS align
    .align \align
    locals_start:
    .set local_ctr, 0
.endm

# Manually does alignment so we can keep track of offsets
.macro local_align align
    .set align_pow, (1 << \align)
    .set align_mask, align_pow - 1
    .set align_off, (align_pow - (local_ctr & align_mask)) & align_mask
    .if align_off != 0
        .space align_off
    .endif
    .set local_ctr, local_ctr + align_off
.endm


.macro local_var name, size, align
    local_align \align
    \name\()_label :
    .set \name, local_ctr
    .space \size
    .set local_ctr, local_ctr + \size
.endm

.macro END_LOCALS
    locals_end:
    .set locals_size, local_ctr
.endm

.macro local_reg_var name
    local_var \name, REG_SIZE, REG_SIZE_BITS
.endm

.macro local_cap_var name
    local_var \name, CAP_SIZE, CAP_SIZE_BITS
.endm

.macro local_cap_var_array, name, n
    local_var \name, (CAP_SIZE * \num), CAP_SIZE_BITS
.endm

.macro local_reg_var_array, name, num
    local_var \name, (REG_SIZE * \num), REG_SIZE_BITS
.endm

.macro local_array_start name, align, len
    local_align \align
    \name\()_start_label :
    .set \name, local_ctr
.endm

.macro local_array_end name, align, len
    local_align \align
    .set \name\()_size, local_ctr - \name
    .set \name\()_total_size, ((\len) * \name\()_size)
    .set extra_size, ((\len - 1) * \name\()_size)
    .set local_ctr, local_ctr + extra_size
    .space extra_size
.endm

#
# A few useful macros
#

# Saves capability register 'creg' to offset 'offset' of frame 'frame'
.macro csave creg, offset, frame
	csc	\creg, $0, \offset*CAP_SIZE+MIPS_FRAME_SIZE(\frame)
.endm

# Restores capability register 'creg' to offset 'offset' of frame 'frame'
.macro crestore creg, offset, frame
	clc	\creg, $0, \offset*CAP_SIZE+MIPS_FRAME_SIZE(\frame)
.endm

# Saves general-purpose register 'greg' to offset 'offset' of frame 'frame'
.macro gsave greg, offset, frame
	csd	\greg, $0, \offset*REG_SIZE(\frame)
.endm

# Restores general-purpose register 'greg' to offset 'offset' of frame 'frame'
.macro grestore greg, offset, frame
	cld	\greg, $0, \offset*REG_SIZE(\frame)
.endm

# Calls 'macro elem, index, global' for each 'elem' of the input list
.macro foreachi macro, index, global, head, tail:vararg
	\macro \head, \index, \global
	.ifnb  \tail
	foreachi \macro, "(\index+1)", \global, \tail
	.endif
.endm


    #
    # Context switch save; step all over k0 and k1 as is usual
    # for MIPS exception handlers.  Notice correspondence to the
    # layout of struct reg_frame.
    #
    # Notice correspondence to the layout of struct reg_frame.
    # Notice also that KCC and KDC are not preserved.
    #

.macro save_reg_frame_idc frame_ptr, tmp, ctmp, pcc, idc
    # Expands to a list of the form
    # csc	$creg, $0, offset*CAP_SIZE+MIPS_FRAME_SIZE(\frame_ptr)
    foreachi csave, 1, \frame_ptr, $c1, $c2, $c3, $c4, $c5, $c6, $c7, $c8, $c9, $c10, $c11, $c12, $c13, $c14, $c15, $c16, $c17, $c18, $c19, $c20, $c21, $c22, $c23, $c24, $c25, \idc, \pcc

    cgetdefault \ctmp
    csave \ctmp, 0, \frame_ptr

    # Expands to a list of the form
    # csd	$greg, $0, offset*REG_SIZE(frame_ptr)
    foreachi gsave, 0, \frame_ptr, $at, $v0, $v1, $a0, $a1, $a2, $a3, $a4, $a5, $a6, $a7, $t0, $t1, $t2, $t3, $s0, $s1, $s2, $s3, $s4, $s5, $s6, $s7, $t8, $t9, $gp, $sp, $fp, $ra
    mfhi	\tmp
    csd	\tmp, $0, 29*REG_SIZE(\frame_ptr)
    mflo	\tmp
    csd	\tmp, $0, 30*REG_SIZE(\frame_ptr)

    dmfc0       \tmp, $4, 2
    csd	        \tmp, $0, 31*REG_SIZE(\frame_ptr)

.endm
.macro save_reg_frame frame_ptr, tmp, c0, pcc
    save_reg_frame_idc \frame_ptr, \tmp, \c0, \pcc, $idc
.endm

    #
    # Restore context from exception frame, which may have had
    # its contents replaced by the kernel.  Notice correspondence
    # to the layout of struct reg_frame.
    #

.macro restore_reg_frame_gen macroc, macrog, frame_ptr, tmp, c0, pcc

    # tmp may be one of the gp registers we restore later - so we must use it for our purposes before gp restore
    cld	\tmp, $0, 29*REG_SIZE(\frame_ptr)
    mthi	\tmp
    cld	\tmp, $0, 30*REG_SIZE(\frame_ptr)
    mtlo	\tmp
    cld \tmp, $0, 31*REG_SIZE(\frame_ptr)
    dmtc0   \tmp, $4, 2

    # Expands to a list of the form
    # cld	$greg, $0, offset*REG_SIZE(\frame_ptr)
    foreachi \macrog, 0, \frame_ptr, $at, $v0, $v1, $a0, $a1, $a2, $a3, $a4, $a5, $a6, $a7, $t0, $t1, $t2, $t3, $s0, $s1, $s2, $s3, $s4, $s5, $s6,	$s7, $t8, $t9, $gp, $sp, $fp, $ra
    #
    # Restore the capability exception frame -- after
    # general-purpose registers, since the above code is done
    # relative to $c0.  Note that, after this point, no further
    # memory access can be done using MIPS load and store
    # instructions.
    # Notice correspondence to the layout of struct reg_frame.
    #
    # Expands to a list of the form
    # clc	$creg, $0, offset*CAP_SIZE+MIPS_FRAME_SIZE(\frame_ptr)
    foreachi \macroc, 0, \frame_ptr, \c0, $c1, $c2, $c3, $c4, $c5, $c6, $c7, $c8, $c9, $c10, $c11, $c12, $c13, $c14, $c15, $c16, $c17, $c18,	$c19, $c20, $c21, $c22, $c23, $c24, $c25, $c26, \pcc

.endm

.macro restore_reg_frame frame_ptr, tmp, c0, pcc
    restore_reg_frame_gen crestore, grestore, \frame_ptr, \tmp, \c0, \pcc
.endm

.macro get_cpu_id reg
    dmfc0   \reg, $15, 1
    andi    \reg, \reg, 0xff
.endm

.macro cgetpccincoffset reg, im
    cgetpcc \reg
    cincoffset  \reg, \reg, (\im + 4)
.endm

.macro cnull reg
    cfromptr    \reg, $idc, $zero
.endm

.macro CRETURN
    ccall $c17, $c18, 2
.endm


.macro for_each_reg caps_rem, macro index, global, head, save_ls:vararg
    .if \caps_rem == 0
        foreachi \macro, \index, \global, \save_ls
    .else
        for_each_reg "(\caps_rem-1)", \macro, \index, \global, \save_ls
    .endif
.endm

.macro for_each_cap caps_rem, macro, index, global, head, tail:vararg
    .if \caps_rem > 0
        \macro \head, \index, \global
        for_each_cap "(\caps_rem-1)", \macro, "(\index+1)", \global, \tail
	.endif
.endm

.macro count reg, index, global
    .set counter, counter+1
.endm

.macro calc_frame n_caps save_ls:vararg
    .set cap_count, \n_caps
    .set counter, 0
    foreachi count, 0, 0, \save_ls
    .set reg_count, counter - \n_caps

    .set cap_sz, (cap_count * CAP_SIZE)
    .set mask, (CAP_SIZE-1)
    .set reg_sz, ((reg_count * REG_SIZE) + mask) & ~mask

    .set frame_sz, cap_sz + reg_sz
.endm

.macro push_cap item, index, global
    csc \item, $zero, (\global + (\index * CAP_SIZE))($c11)
.endm
.macro push_reg item, index, global
    csd \item, $zero, (\global + (\index * REG_SIZE))($c11)
.endm
.macro pop_cap item, index, global
    clc \item, $zero, (\global + (\index * CAP_SIZE))($c11)
.endm
.macro pop_reg item, index, global
    cld \item, $zero, (\global + (\index * REG_SIZE))($c11)
.endm


.macro FRAME_PUSH n_caps, save_ls:vararg
    calc_frame \n_caps, \save_ls
    cincoffset $c11, $c11, -(frame_sz)
    for_each_cap \n_caps, push_cap, 0, 0, \save_ls
    for_each_reg \n_caps, push_reg, 0, (cap_sz), \save_ls
.endm


.macro FRAME_POP n_caps, save_ls:vararg
    calc_frame \n_caps, \save_ls
    for_each_cap \n_caps, pop_cap, 0, 0, \save_ls
    for_each_reg \n_caps, pop_reg, 0, (cap_sz), \save_ls
    cincoffset $c11, $c11, (frame_sz)
.endm

.macro POP_AND_RETURN n_caps, save_ls:vararg
    calc_frame \n_caps, \save_ls
    for_each_cap \n_caps, pop_cap, 0, 0, \save_ls
    for_each_reg \n_caps, pop_reg, 0, (cap_sz), \save_ls
    ccall $c17, $c18, 2
    cincoffset $c11, $c11, (frame_sz)
.endm