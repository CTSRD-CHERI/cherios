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

    # Set KSU = 0
    li      $t1, (0b11 << 3)
    not     $t1, $t1
    and      $t0, $t0, $t1

    # Set UX, SX, and KSX
    li      $t1, (0b111 << 5)
    or      $t0, $t0, $t1

    mtc0		$t0, $12

    # Set revoking nothing

    dmtc0  $zero, $30, 4
    dmtc0  $zero, $30, 2
    dmtc0  $zero, $30, 3

    .set N_TLB_ENTS, 32
    # Setup the TLB so it won't break on first miss
    dli     $t1, N_TLB_ENTS

1:
    move    $t0, $t1
    dmtc0   $t0, $0
    dmtc0   $t0, $10        # cretae a VPN 0, and a ASID of 77+i so it doesn't match anything else
    tlbwi
    bnez    $t1, 1b
    daddiu  $t1, -1

	# Wait for capability coprocessor to be enabled for real
	nop; nop; nop; nop; nop; nop; nop; nop;

	# Clear other capability registers to ensure we don't retain any
	# executable capabilities: everything but $pcc and $c0.
	CClearLo	0xfffe
	CClearHi	0xffff

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

    # Nano kernel init will give us a pointer to its plt, and a default data capability
    cmove       $c18, $c1
    cmove       $c19, $c2
    cmove       $c20, $c3
	# Set up a boot-time stack using memory allocated by the linker;
	# assume that $c0 is a sensible starting capability for this.
	dla		$t0, __start_boot_stack
	dla		$t1, __size_boot_stack
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

	# We pass the nano kernel if / data cap and handle for the first context
	dli		$a0, 0
    cmove   $c3, $c20
    cmove   $c4, $c18
    cmove   $c5, $c19

	# Call bootloader_main().
	cgetpcc		$c12
	dla		$t0, bootloader_main
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# Bootloader done. Go in waiting state. #fixme: revoke object
	cfromptr $c1, $c1, $0
	creturn
