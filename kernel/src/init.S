#-
# Copyright (c) 2016 Hongyan Xia
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

.set MIPS_SZCAP, _MIPS_SZCAP
.include "asm.S"

# Assembly stub on the way to running C code.  Assumes that $c0 and $pcc are
# global and suitable.  Enable capability coprocessor.  Set up a stack.  Jump
# to various startup functions to initialise the C runtime, and then call
# cherios_main().

	.text
	.section .init
	.global start
	.ent start
start:
	#Add 4 to EPCC
	dmfc0	$k1, $14
	addi	$k1, $k1, 4
	dmtc0	$k1, $14

	#Save register frame
	dla	$k0, kernel_init_save_frame

    sd $at, 0*REG_SIZE($k0)
    sd $v0, 1*REG_SIZE($k0)
    sd $v1, 2*REG_SIZE($k0)
    sd $a0, 3*REG_SIZE($k0)
    sd $a1, 4*REG_SIZE($k0)
    sd $a2, 5*REG_SIZE($k0)
    sd $a3, 6*REG_SIZE($k0)
    sd $t0, 7*REG_SIZE($k0)
    sd $t1, 8*REG_SIZE($k0)
    sd $s0, 9*REG_SIZE($k0)
    sd $s1, 10*REG_SIZE($k0)
    sd $s2, 11*REG_SIZE($k0)
    sd $s3, 12*REG_SIZE($k0)
    sd $s4, 13*REG_SIZE($k0)
    sd $t9, 14*REG_SIZE($k0)
    sd $gp, 15*REG_SIZE($k0)
    sd $sp, 16*REG_SIZE($k0)
    sd $fp, 17*REG_SIZE($k0)
    sd $ra, 18*REG_SIZE($k0)
    mfhi	$k1
    sd	$k1, 19*REG_SIZE($k0)
    mflo	$k1
    sd	$k1, 20*REG_SIZE($k0)
    dmfc0	$k1, $14		# EPC
    sd	$k1, 21*REG_SIZE($k0)

	# Set up a boot-time stack using memory allocated by the linker;
	# assume that $c0 is a sensible starting capability for this.
	dla		$sp, __stop_exception_stack

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

	# Pass arguments to cherios_main(int, void*) in a0 and c3.
	dli		$a0, 1
	ld	$a1, 4*REG_SIZE($k0)

	# Call cherios_main().
	dla		$t9, cherios_main
	jalr		$t9
	nop

	# Restore context from the register frame set by the scheduler,
	# typically pointing to an init activation.
	dla	$k0, kernel_exception_framep_ptr
    ld  $k0, 0($k0)

	# Expands to a list of the form
	# cld	$greg, $0, offset*REG_SIZE($kr1c)
    ld $at, 0*REG_SIZE($k0)
    ld $v0, 1*REG_SIZE($k0)
    ld $v1, 2*REG_SIZE($k0)
    ld $a0, 3*REG_SIZE($k0)
    ld $a1, 4*REG_SIZE($k0)
    ld $a2, 5*REG_SIZE($k0)
    ld $a3, 6*REG_SIZE($k0)
    ld $t0, 7*REG_SIZE($k0)
    ld $t1, 8*REG_SIZE($k0)
    ld $s0, 9*REG_SIZE($k0)
    ld $s1, 10*REG_SIZE($k0)
    ld $s2, 11*REG_SIZE($k0)
    ld $s3, 12*REG_SIZE($k0)
    ld $s4, 13*REG_SIZE($k0)
    ld $t9, 14*REG_SIZE($k0)
    ld $gp, 15*REG_SIZE($k0)
    ld $sp, 16*REG_SIZE($k0)
    ld $fp, 17*REG_SIZE($k0)
    ld $ra, 18*REG_SIZE($k0)
    ld	$k1, 19*REG_SIZE($k0)
    mthi	$k1
    ld	$k1, 20*REG_SIZE($k0)
    mtlo	$k1
    ld	$k1, 21*REG_SIZE($k0)
    dmtc0	$k1, $14		# EPC

	# Kernel init done
	move	$k0, $zero
	move	$k1, $zero
	eret
start_end:
	.size start, start_end - start
	.end start

	.align 5
kernel_init_save_frame:
	.skip (32*REG_SIZE), 0 #Should be enough
kernel_init_save_frame_end:
	.size kernel_init_save_frame, kernel_init_save_frame_end - kernel_init_save_frame
