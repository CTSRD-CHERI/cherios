# -
# Copyright (c) 2018 Lawrence Esswood
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
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
.set MIPS_SZCAP, _MIPS_SZCAP
#include "mips.h"
.include "asm.S"
#include "dylink.h"

.set RED_ZONE, 256

# Need to save all args regs and tmps. Look in exceptions.h for structure.
# If the user wants to change these values he can.


#define REG_LIST \
    $at, $v0, $v1,                          \
    $a0, $a1, $a2, $a3, $a4, $a5, $a6, $a7, \
    $t0, $t1, $t2, $t3,                     \
    $t8, $t9,                               \
    $gp, $sp, $fp, $ra

.set V0_index, 1
.set V1_index, 2
.set AT_index, 0
.set c18_index, 14
.set c17_index, 13

.set CAP_START, (24 * REG_SIZE)

#define CAP_LIST     \
    $c2, $c3, $c4, $c5, $c6, $c7, $c8, \
    $c9, $c12, $c13, $c14, $c15, $c16, $c17, $c18, $c25

.macro save_cap reg, index, b
    cscbi \reg, (CAP_START + (CAP_SIZE * \index))($c11)
.endm
.macro save_reg reg, index, b
    csd \reg, $zero, (\index * REG_SIZE)($c11)
.endm

.macro save_other_reg reg, index, b
    .if \index != V0_index
        .if  \index != V1_index
            .if \index != AT_index
                save_reg \reg, \index, \b
            .endif
        .endif
    .endif
.endm

.macro save_other_cap reg, index, b
    .if \index != c18_index && \index != c17_index
        save_cap \reg, \index, \b
    .endif
.endm

.macro load_cap reg, index, b
    clcbi \reg, (CAP_START + (CAP_SIZE * \index))($c11)
.endm

.macro load_reg reg, index, b
    .if \index != V0_index
        cld \reg, $zero, (\index * REG_SIZE)($c11)
    .endif
.endm

.set SAVE_SIZE, ((14 * CAP_SIZE) + (CAP_START))
.set STACK_INC, RED_ZONE + SAVE_SIZE




.text

.global user_exception_trampoline_vector
.ent user_exception_trampoline_vector

user_exception_trampoline_vector:

cincoffset  $c11, $c11, -(STACK_INC)

# Need to save something for scratch space
save_cap    $c18, c18_index, 0
save_cap    $c17, c17_index, 0
save_reg    $v0, V0_index, 0
save_reg    $v1, V1_index, 0
save_reg    $at, AT_index, 0

cmove       $c18, $idc
clcbi       $c1, CTLP_OFFSET_CGP($idc)
clcbi       $idc, %captab20(nano_kernel_if_t_data_obj)($c1)
clcbi       $c1, %captab20(exception_getcause)($c1)
clcbi       $c1, (32)($c1) # Get the code capability directly from the plt stub

# Call get cause
# Gets v0 (cause) and v1(ccause). clobers c1. Restores idc. 18 now available as tmp.
ccall       $c1, $idc, 2
cgetpcc     $c17


clcbi       $c1, CTLP_OFFSET_CGP($idc)                          # $c1 is now our globals

andi        $at, $v0, (MIPS_CP0_CAUSE_EXCCODE)
daddiu      $at, $at, -(MIPS_CP0_EXCODE_C2E << MIPS_CP0_CAUSE_EXCODE_SHIFT)
bnez        $at, vector_cap_exc_failed
daddiu      $at, $at, (MIPS_CP0_EXCODE_C2E << MIPS_CP0_CAUSE_EXCODE_SHIFT)

andi        $at, $v1, 0x1F00
dsrl        $at, $at, (8 - CAP_SIZE_BITS)
clcbi       $c18, %captab20(chandle_vector)($c1)
clc         $c18, $at, 0($c18)
cbts        $c18, user_c_exception_common_entry
daddiu      $at, $zero, (MIPS_CP0_EXCODE_C2E << MIPS_CP0_CAUSE_EXCODE_SHIFT)

// It wasnt a cap exception or no cap at correct entry in cap exception vector
vector_cap_exc_failed:

dsll        $at, $at, (CAP_SIZE_BITS - MIPS_CP0_CAUSE_EXCODE_SHIFT)
clcbi       $c18, %captab20(handle_vector)($c1)
clc         $c18, $at, 0($c18)
cbts        $c18, user_c_exception_common_entry
nop

trampoline_not_set:

load_cap    $c18, c18_index, 0
load_cap    $c17, c17_index, 0
load_reg    $v1, V1_index, 0
load_reg    $at, AT_index, 0
clcbi       $idc, %captab20(nano_kernel_if_t_data_obj)($c1)
cincoffset $c11, $c11, (STACK_INC)
b           replay_return
cld         $v0, $zero, (-STACK_INC + (V0_index * REG_SIZE))($c11)

user_c_exception_common_entry:

foreachi    save_other_cap, 0, 0, CAP_LIST
foreachi    save_other_reg, 0, 0, REG_LIST
mfhi        $t0
save_reg    $t0, 21, 0
mflo        $t0
save_reg    $t0, 22, 0

# Put globals in a conventional register
cmove   $c25, $c1

move    $a0, $v0
move    $a1, $v1
cmove   $c3, $c11
cmove   $c12, $c18

# Call users exception routine

call_func_reg

# Restore everything (but v0 which contains what to do next)

load_reg    $t0, 21, 0
mthi        $t0
load_reg    $t0, 22, 0
mtlo        $t0

foreachi    load_cap, 0, 0, CAP_LIST
foreachi    load_reg, 0, 0, REG_LIST

cincoffset  $c11, $c11, (STACK_INC)

clcbi       $c1, CTLP_OFFSET_CGP($idc)                          # $c1 is now our globals
clcbi       $idc, %captab20(nano_kernel_if_t_data_obj)($c1)

bnez        $v0, replay_return
# Perform this last restore in the delay slot. pcc now efectively encodes which return to use
cld         $v0, $zero, (-STACK_INC + (V0_index * REG_SIZE))($c11)

# We do the ccall ourselves as we have to be very careful all registers are restored
# Only pcc/idc/c1 are restored by the nano kernel, so these are all we can use

normal_return:

# We need access to globals for nano kernel calls - just pretend they are thread locals
# So we can use the macros

# LLVM is really stupid, we can't use idc for non thread local things - have to use c1

clcbi       $c1, %captab20(exception_return)($c1)
clcbi       $c1, (32)($c1) # Get the code capability directly from the plt stub
ccall       $c1, $idc, 2
nop

replay_return:

clcbi       $c1, %captab20(exception_replay)($c1)
clcbi       $c1, (32)($c1) # Get the code capability directly from the plt stub
ccall       $c1, $idc, 2
nop

.end user_exception_trampoline_vector
