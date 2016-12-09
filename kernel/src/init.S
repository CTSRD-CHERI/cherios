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
	dla	$k1, kernel_init_save_frame_end
	cgetdefault $kr1c
	csetoffset $kr1c, $kr1c, $k0
	sub	$k0, $k1, $k0
	csetbounds $kr1c, $kr1c, $k0

	cgetdefault	$kr2c
	# Expands to a list of the form
	# csc	$creg, $0, offset*CAP_SIZE+MIPS_FRAME_SIZE($kr1c)
	foreachi csave, 0, $kr1c,					\
		$kr2c, $c1, $c2, $c3, $c4, $c5, $c6, $c7, $c8, $c9,	\
		$c10, $c11, $c12, $c13, $c14, $c15, $c16, $c17, $c18,	\
		$c19, $c20, $c21, $c22, $c23, $c24, $c25, $c26, $c31
	# Expands to a list of the form
	# csd	$greg, $0, offset*REG_SIZE($kr1c)
	foreachi gsave, 0, $kr1c,					\
		$at, $v0, $v1, $a0, $a1, $a2, $a3, $a4, $a5, $a6, $a7,	\
		$t0, $t1, $t2, $t3, $s0, $s1, $s2, $s3, $s4, $s5, $s6,	\
		$s7, $t8, $t9, $gp, $sp, $fp, $ra
	mfhi	$k1
	csd	$k1, $0, 29*REG_SIZE($kr1c)
	mflo	$k1
	csd	$k1, $0, 30*REG_SIZE($kr1c)
	dmfc0	$k1, $14		# EPC
	csd	$k1, $0, 31*REG_SIZE($kr1c)


	# Set up a boot-time stack using memory allocated by the linker;
	# assume that $c0 is a sensible starting capability for this.
	dla		$t0, __stop_exception_stack
	move		$sp, $t0

	# Initialise BSS and globals before running C constructors, so that C
	# constructors can take them (and other aspects of the conventional C
	# runtime) for granted.

	# Zero BSS.
	dla		$t0, crt_init_bss
	dla		$t9, crt_init_bss
    jalr    $t0
	nop

	# Initialise globals.
	dla		$t0, crt_init_globals
	dla		$t9, crt_init_globals
    jalr    $t0
	nop

	# Call C constructors.
	dla		$t0, crt_call_constructors
	dla		$t9, crt_call_constructors
    jalr    $t0
	nop

	# Pass arguments to cherios_main(int, void*) in a0 and c3.
	dli		$a0, 1
	cld	$a1, $0, 4*REG_SIZE($kr1c)

	# Call cherios_main().
	dla		$t0, cherios_main
	dla		$t9, cherios_main
	jalr		$t0
	nop

	# Restore context from the register frame set by the scheduler,
	# typically pointing to an init activation.
	dla	$k0, kernel_exception_framep_ptr
	clc	$kr1c, $k0, 0($kdc)

	# Expands to a list of the form
	# cld	$greg, $0, offset*REG_SIZE($kr1c)
	foreachi grestore, 0, $kr1c,					\
		$at, $v0, $v1, $a0, $a1, $a2, $a3, $a4, $a5, $a6, $a7,	\
		$t0, $t1, $t2, $t3, $s0, $s1, $s2, $s3, $s4, $s5, $s6,	\
		$s7, $t8, $t9, $gp, $sp, $fp, $ra
	cld	$k1, $0, 29*REG_SIZE($kr1c)
	mthi	$k1
	cld	$k1, $0, 30*REG_SIZE($kr1c)
	mtlo	$k1
	cld	$k1, $0, 31*REG_SIZE($kr1c)
	dmtc0	$k1, $14		# EPC
	# Expands to a list of the form
	# clc	$creg, $0, offset*CAP_SIZE+MIPS_FRAME_SIZE($kr1c)
	foreachi crestore, 0, $kr1c,					\
		$kr2c, $c1, $c2, $c3, $c4, $c5, $c6, $c7, $c8, $c9,	\
		$c10, $c11, $c12, $c13, $c14, $c15, $c16, $c17, $c18,	\
		$c19, $c20, $c21, $c22, $c23, $c24, $c25, $c26, $c31
	csetdefault	$kr2c

	# Kernel init done
	move	$k0, $zero
	move	$k1, $zero
	eret
start_end:
	.size start, start_end - start
	.end start

	.align 5
kernel_init_save_frame:
	.skip 32*CAP_SIZE + 32*REG_SIZE, 0 #Should be enough
kernel_init_save_frame_end:
	.size kernel_init_save_frame, kernel_init_save_frame_end - kernel_init_save_frame
