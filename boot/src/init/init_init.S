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

.set noreorder
.set nobopt
.set noat

# Assembly stub on the way to running C code.  Assumes that $c0 and $pcc are
# global and suitable.  Enable capability coprocessor.  Set up a stack.  Jump
# to various startup functions to initialise the C runtime, and then call
# cherios_main().

	.text
	.global start
	.ent start
start:
	# W^X for pointers: ensure $c0 is never executable.
	cgetdefault	$c3
	dli		$t0, ~(1<<1)
	candperm	$c3, $c3, $t0
	csetdefault	$c3

	# Implement W^X for pointers: trim store permissions from $pcc to
	# prevent it from becoming writable in the future.
	cgetpcc		$c12
	dli		$t0, ~((1<<3) | (1<<5))
	candperm	$c12, $c12, $t0

	# Jump to write-free $pcc.
	dla		$t0, cherios_init_jump_target
	csetoffset	$c12, $c12, $t0
	cjr		$c12
	nop
cherios_init_jump_target:
	# Set up a stack using memory allocated by the linker;
	# assume that $c0 is a sensible starting capability for this.
	dla		$t0, __start_init_stack
	dla		$t1, __size_init_stack
	cgetdefault	$c11
	csetoffset	$c11, $c11, $t0
	csetbounds	$c11, $c11, $t1
	move		$sp, $t1

	# Initialise BSS and globals before running C constructors, so that C
	# constructors can take them (and other aspects of the conventional C
	# runtime) for granted.

	# Zero BSS.
	cgetpcc		$c12
	dla		$t0, crt_init_bss
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# Initialize the init_info pointer from $c23
	cgetdefault	$c12
	dla		$t0, init_info
	csc		$c23, $t0, 0($c12)

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

	# On the off chance that cherios_main() has been defined as having
	# arguments, set the first argument (argc) to 0.
	dli		$a0, 0

	# Call init_main().
	cgetpcc		$c12
	dla		$t0, init_main
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# Bootloader done. Go in waiting state. #fixme: revoke object
	cfromptr $c1, $c1, $0
	creturn
