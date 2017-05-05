#-
# Copyright (c) 2016 Robert N. M. Watson
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

.set noreorder
.set nobopt
.set noat

# Assembly stub on the way to running C code.  Assumes that $c0 and $pcc are
# global and suitable.  Jump to various startup functions to initialise the C
# runtime, and then call main().

	.text
	.global start_common
	.ent start_common
start_common:
	# Save arguments
	move		$s0, $a0
	move		$s1, $a1
	cmove		$c17, $c3 # return address set to null
	cmove		$c18, $c4 # set to null
	cmove		$c19, $c5 # self_ctrl

	# Initialise BSS and globals before running C constructors, so that C
	# constructors can take them (and other aspects of the conventional C
	# runtime) for granted.

	# Zero BSS.
	cgetpcc		$c12
	dla		$t0, crt_init_bss
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# Initialise globals.
	cgetpcc		$c12
	dla		$t0, crt_init_globals
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# Call C constructors.
	cgetpcc		$c12
	dla		$t0, crt_call_constructors
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# Call object init.
	cmove		$c3, $c19 #self_ctrl
	cmove		$c4, $c22 #self_cap
	cmove		$c5, $c23 #ns_ref
	cmove		$c6, $c24 #kernel_if
	cmove       $c7, $c25 #queue

	cgetpcc		$c12
	dla		$t0, libuser_init
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# Restore arguments
	move		$a0, $s0
	move		$a1, $s1
	cmove		$c3, $c17
	cmove		$c4, $c18

	# Call main().
	cgetpcc		$c12
	dla		$t0, main
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# main() done. Exit or CReturn
	dla		$t0, msg_enable
	cgetdefault	$c17
	cld		$t0, $t0, 0($c17)
	bne		$t0, $zero, msg_entry
	nop
	dla		$t0, act_self_ctrl
	clc		$c3, $t0, 0($c17)
	li		$v0, 26
	syscall