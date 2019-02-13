#-
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

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
#include "asm.S"

.set noreorder
.set nobopt
.set noat

	.section init, "ax"
	.global start
	.ent start
start:
    # Save argumments to init
    cmove       $c19, $c3
    move		$s0, $a0

    # We will not have a stack set up because bootstrapping is hard.
    # Rather than have the kernel do it, its going to be done here

    dla         $t0, __start_init_stack
    cgetdefault $c11
    dla         $t1, __size_init_stack
    csetoffset  $c11, $c11, $t0
    csetbounds  $c11, $c11, $t1
    csetoffset  $c11, $c11, $t1

	# Initialise globals. We need to use dla to call this function. Can only do this because dla was loaded contiguously
	cgetpcc		$c12
	dla		$t0, crt_init_globals_init
	csetoffset	$c12, $c12, $t0
	csetbounds  $idc, $c11, $zero
	cjalr		$c12, $c17
	cmove   $c18, $idc

	cmove   $idc, $c3

    # Zero BSS.
    call_func crt_init_bss

    # Call C constructors.
    call_func crt_call_constructors

	# Call object init.
	cmove		$c3, $c21 # self_ctrl
	cfromptr	$c4, $c4, $zero # ns_ref (NULL)
	cmove		$c5, $c24 # kernel_if
	cmove       $c6, $c20 # queue
	cmove       $c7, $c4  # process ref (NULL)
    cmove       $c8, $c4  # MOP         (NULL)
    cnull       $c9       # cdl tres
    li          $a0, 0    # flags (NONE)
    call_func   libuser_init

	# Restore arguments
	move		$a0, $s0
    cmove       $c3, $c19
    cgetpccsetoffset $c4, $zero                     # Need this to operate a tmp alloc pool
	# Call main().
	call_func   main

    # Inits main never returns

    .end start