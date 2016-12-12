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
	# Enable capability coprocessor.
	mfc0		$t0, $12
#ifdef HARD_FLOAT
	li		$t1, 0x64000000
#else
	li		$t1, 0x40000000
#endif
	or		$t0, $t0, $t1
	# Clear ERL
	li		$t1, 0x4
	nor		$t1, $t1, $t1
	and		$t0, $t0, $t1
	mtc0		$t0, $12

	# Wait for capability coprocessor to be enabled for real
	nop; nop; nop; nop; nop; nop; nop; nop;

	# Clear other capability registers to ensure we don't retain any
	# executable capabilities: everything but $pcc and $c0.
	CClearLo	0xfffe
	CClearHi	0xffff

	# Jump to write-free $pcc.
	dla		$t0, cherios_init_jump_target
	jalr		$t0
	nop
cherios_init_jump_target:

	# Populate exception registers: $kdc and $kcc
	cgetpcc		$c29
	cgetdefault	$c30

	# Set up a boot-time stack using memory allocated by the linker;
	# assume that $c0 is a sensible starting capability for this.
	dla		$t0, __stop_boot_stack
	move		$sp, $t0

	# Initialise BSS and globals before running C constructors, so that C
	# constructors can take them (and other aspects of the conventional C
	# runtime) for granted.

	# Zero BSS.
	dla		$t9, crt_init_bss
    jalr    $t9
	nop

	# Initialise globals.
	dla		$t9, crt_init_globals
    jalr    $t9
	nop

	# Call C constructors.
	dla		$t9, crt_call_constructors
    jalr    $t9
	nop

	# On the off chance that cherios_main() has been defined as having
	# arguments, set the first argument (argc) to 0.
	dli		$a0, 0

	# Call cherios_main().
	dla		$t9, cherios_main
    jalr    $t9
	nop

	# Bootloader done. Go in waiting state. #fixme: revoke object
    move    $v0, $0
#jr  $ra
    li $v1, 1010
    syscall
    nop
