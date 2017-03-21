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

.set CAP_SIZE, (MIPS_SZCAP/8)
.set REG_SIZE, (64/8)
.set MIPS_FRAME_SIZE, (32*REG_SIZE)
.set REG_SIZE_BITS, 3
.set CHERI_CAP_FRAME_SIZE, (27 * CAP_SIZE)
.set CHERI_FRAME_SIZE, (MIPS_FRAME_SIZE + CHERI_CAP_FRAME_SIZE)
.set FRAME_C3_OFFSET, MIPS_FRAME_SIZE + (3 * CAP_SIZE)

.ifeq MIPS_SZCAP-256
.set CAP_SIZE_BITS, 5
.else
.ifeq MIPS_SZCAP-128
.set CAP_SIZE_BITS, 4
.else
.error "Unknown cap size"
.endif
.endif


.set Perm_Global, 0
.set Perm_Execute, 1
.set Perm_Load, 2
.set Perm_Store, 3
.set Perm_Load_Capability, 4
.set Perm_Store_Capability, 5
.set Perm_Store_Local_Capability, 6
.set Perm_Seal, 7
.set Perm_Access_System_Registers, 10
.set Perm_All, (1 << 10) - 1

.macro local_var name, size, align
    .align \align
    \name\()_label : .space \size
    .set \name, 0 # \name\()_label - locals_start
.endm

.macro local_reg_var name
    local_var \name, REG_SIZE, REG_SIZE_BITS
.endm

.macro local_cap_var name
    local_var \name, CAP_SIZE, CAP_SIZE_BITS
.endm

.data

.set noreorder
.set nobopt
.set noat

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

.macro save_reg_frame_idc frame_ptr, tmp, c0, pcc, idc
    # Expands to a list of the form
    # csc	$creg, $0, offset*CAP_SIZE+MIPS_FRAME_SIZE(\frame_ptr)
    foreachi csave, 0, \frame_ptr, \c0, $c1, $c2, $c3, $c4, $c5, $c6, $c7, $c8, $c9, $c10, $c11, $c12, $c13, $c14, $c15, $c16, $c17, $c18, $c19, $c20, $c21, $c22, $c23, $c24, $c25, \idc, \pcc


    # Expands to a list of the form
    # csd	$greg, $0, offset*REG_SIZE(frame_ptr)
    foreachi gsave, 0, \frame_ptr, $at, $v0, $v1, $a0, $a1, $a2, $a3, $a4, $a5, $a6, $a7, $t0, $t1, $t2, $t3, $s0, $s1, $s2, $s3, $s4, $s5, $s6, $s7, $t8, $t9, $gp, $sp, $fp, $ra
    mfhi	\tmp
    csd	\tmp, $0, 29*REG_SIZE(\frame_ptr)
    mflo	\tmp
    csd	\tmp, $0, 30*REG_SIZE(\frame_ptr)

    cgetoffset	\tmp, \pcc
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