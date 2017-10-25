# -
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

//FIXME set this properly
.set MIPS_SZCAP, _MIPS_SZCAP
.include "asm.S"

#define __ASSEMBLY__ 1
#include "activations.h"

# Stack transition for trampolines. Needed for any kernel ccallee c functions
# Only needed if we need a stack.

.text

# Has a weird ABI due to having to swap registers. We use c16 instead of c12 and c14 instead of c17
# On return we will have conventional register usage back

.global kernel_ccall_stack_swap
        kernel_ccall_stack_swap:
		li     $t1, (USER_KERNEL_STACK_SIZE) # Also points to our entry guard variable

        # Sets guard to 0 -> 1
        cincoffset  $c15, $idc, $t1
        clld        $t2, $c15
        bnez        $t2, kernel_ccall_entry_er
        li          $t2, 1
        cscd        $t2, $t2, $c15
        beqz        $t2, kernel_ccall_entry_er

		csetbounds $c15, $idc, $t1 # our stack is in the activation struct pointed to idc
        clc        $c16, $t1, -1 * CAP_SIZE($c15)   # pop c0 from the stack

		# push callers c0, spc, rac, and sp
		cgetdefault $c13
		csc    $c13, $t1, -1 * CAP_SIZE($c15)
		csc    $c17, $t1, -2 * CAP_SIZE($c15)
		csc    $c12, $t1, -3 * CAP_SIZE($c15)
		csc    $c11, $t1, -4 * CAP_SIZE($c15)
        csc    $idc, $t1, -5 * CAP_SIZE($c15)
		csd    $sp,  $t1, -6 * CAP_SIZE($c15)

		# now setup our conventional c0, spc, rac and sp
		csetdefault $c16                  # New c0
		cmove       $c11, $c15            # New spc
		daddiu  $sp, $t1, -(6 * CAP_SIZE) # New sp

        cjr     $c14

#TODO we only clear capabilities here. If we care about information leak we may wish to be stricter.

.global kernel_ccall_stack_unswap
        kernel_ccall_stack_unswap:

		# pop callers c0, spc, rac, and sp
		# We need to take a copy c11 as we will be overwriting it
		cmove  $c15, $c11
		clc    $c13, $sp, 5 * CAP_SIZE($c15)
		cgetdefault $c14
		csetdefault $c13                            # push c0 back to kernel stack
		csc    $c14, $sp, 5 * CAP_SIZE($c15)

		clc    $c17, $sp, 4 * CAP_SIZE($c15)
		clc    $c12, $sp, 3 * CAP_SIZE($c15)
		clc    $c11, $sp, 2 * CAP_SIZE($c15)
		clc    $idc, $sp, 1 * CAP_SIZE($c15)
		cld    $sp,  $sp, 0 * CAP_SIZE($c15)

        # sets guard 1 -> 0
        li     $t1, (USER_KERNEL_STACK_SIZE)
        csd    $zero, $t1, 0($idc)

# Our ccall convetion allows multiple entries concurrently - which would break the kernel
# In order to enforce 'one caller at once' semantics we have a guard variable
kernel_ccall_entry_er:
#TODO when the day comes and we care, REALLY check these are all the registers to be cleared
		# Clear everything caller saved ($c1 to $c16 minus 3, 11 and 12)
		cclearlo   (0b1110011111110110)
		cmove      $c16, $c1

        ccall $c17, $c18, 2
        nop