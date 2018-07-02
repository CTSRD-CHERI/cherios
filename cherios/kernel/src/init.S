#-
# Copyright (c) 2016 Robert N. M. Watson
# Copyright (c) 2016 Hadrien Barral
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

#Fixme: clean-up code && comment

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
#include "asm.S"
#include "mips.h"

# Assembly stub on the way to running C code.  Assumes that $c0 and $pcc are
# global and suitable.  Enable capability coprocessor.  Set up a stack.  Jump
# to various startup functions to initialise the C runtime, and then call
# cherios_main().

	.text
	.section .init
	.global start
	.ent start
start:
    cmove       $c19, $c1   # if
    cmove       $c20, $c2   # data
    cmove       $c21, $c3   # context
    move        $s0,  $a0
    move        $s1,  $a1
    move        $s2,  $a2
    move        $s3,  $a3

	# Set up an exception stack using memory allocated by the linker;
	# assume that $c0 is a sensible starting capability for this.
	dla		$t0, __start_exception_stack
	dla		$t1, __size_exception_stack
	cgetdefault	$c11
	csetoffset	$c11, $c11, $t0
	csetbounds	$c11, $c11, $t1
	csetoffset  $c11, $c11, $t1

	# Initialise BSS and globals before running C constructors, so that C
	# constructors can take them (and other aspects of the conventional C
	# runtime) for granted.

	# Initialise globals. We need to use dla to call this function. Can only do this because dla was loaded contiguously
	cgetpcc		$c12
	dla		$t0, crt_init_globals_kernel
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	cmove   $c18, $c11

	cmove   $idc, $c3

	# Zero BSS.
	call_func crt_init_bss

	# Call C constructors.
	call_func crt_call_constructors

	move		$a0, $s0
	move        $a1, $s1
	move        $a2, $s2
	move        $a3, $s3
    cmove       $c3, $c19
    cmove       $c4, $c20
    cmove       $c5, $c21
    cgetdefault $c6                 # Need this to auth overwriting of the PLT
    cgetpccsetoffset $c7, $zero     # Need this to construct a pcc for init in act_init
	# Call cherios_main() this should never return as the kernel will set itself up as the exception context.
	call_func   cherios_main

start_end:
	.size start, start_end - start
	.end start

	.align 5