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


.set CAP_START, (24 * REG_SIZE)

#define CAP_LIST     \
    $c2, $c2, $c3, $c4, $c5, $c6, $c7, $c8, \
    $c9, $c12, $c13, $c14, $c15, $c16, $c17, $c18, $c25

.macro save_cap reg, index, b
    cscbi \reg, (CAP_START + (CAP_SIZE * \index))($c11)
.endm
.macro save_reg reg, index, b
    csd \reg, $zero, (\index * REG_SIZE)($c11)
.endm

.macro load_cap reg, index, b
    clcbi \reg, (CAP_START + (CAP_SIZE * \index))($c11)
.endm


.set V0_index, 1
.macro load_reg reg, index, b
    .if \index != V0_index
        cld \reg, $zero, (\index * REG_SIZE)($c11)
    .endif
.endm

.set SAVE_SIZE, ((14 * CAP_SIZE) + (CAP_START))
.set STACK_INC, RED_ZONE + SAVE_SIZE




.text

.global user_exception_trampoline
.ent user_exception_trampoline
user_exception_trampoline:

# On entry here c1 is a scratch, and idc will be our local cap table
# We will assume the stack is somewhat available.

# TODO we may not be able to trust the stack is non null. We might have to load it from idc

cincoffset $c11, $c11, -(STACK_INC)

# Save everything

foreachi    save_cap, 0, 0, CAP_LIST
foreachi    save_reg, 0, 0, REG_LIST
mfhi        $t0
save_reg    $t0, 21, 0
mflo        $t0
save_reg    $t0, 22, 0

# Load globals
clcbi       $c25, CTLP_OFFSET_CGP($idc)

# Call get cause - gets v0 and v1

call_func exception_getcause

move    $a0, $v0
move    $a1, $v1
cmove   $c3, $c11

# Call users exception routine

get_sym_val $c12, current_handler, c, $c12
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

clcbi       $c1, %captab20(exception_replay)($idc)
clcbi       $c1, (32)($c1) # Get the code capability directly from the plt stub
ccall       $c1, $idc, 2
nop

.end user_exception_trampoline
