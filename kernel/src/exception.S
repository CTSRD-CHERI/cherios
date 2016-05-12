#-
# Copyright (c) 2011 Robert N. M. Watson
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
.set CAP_SIZE, (256/8)
.set REG_SIZE, (64/8)

#
# Capability-aware exception handler.
#
# Code in this file assumes that the Deimos kernel executes in a single global
# memory segment that will be described by KCC (and installed in PCC during
# entry to the exception vector) and KDC.  It is possible to imagine linking
# the kernel so that KCC describes only code, and KDC describes only data, but
# we don't yet do this.
#
# Exception code can safely use $k0, $k1, $kdc, $kcc, and $c30 to perform its
# context switch.  PC and PCC are both implicitly used, with pre-exception
# values preserved in EPC and EPCC respectively.
#

#
# Relocatable exception vector; jumps back to kernel_exception_handler() in
# normal program memory.  This runs with KCC installed in PCC.
#
		.global kernel_exception_trampoline
		.ent kernel_exception_trampoline
kernel_exception_trampoline:
		dla	$k0, kernel_cp2_exception_handler
		jr	$k0
		nop
kernel_exception_trampoline_end:
		nop
		.global kernel_exception_trampoline_end
		.end kernel_exception_trampoline
		.size kernel_exception_trampoline, kernel_exception_trampoline_end - kernel_exception_trampoline
		.size kernel_exception_trampoline_end, 1 # 0 triggers capsizefix warning

#
# Assembly-level exception handler routine -- save context, invoke
# kernel_exception() with a reference to the frame, then restore context and
# return from the exception handler.
#
# XXXRW: Note that we use a global pointer for the exception frame, so this is
# inherrently single-CPU.
#
		.text
		.global kernel_cp2_exception_handler
		.ent kernel_cp2_exception_handler
kernel_cp2_exception_handler:

		#
		# Since we are still running with user capabilities in most
		# capability registers, we must access all kernel data
		# relative to KDC.  First save and clear CP2 context, then
		# move on to general-purpose registers.
		#
		# Notice correspondence to the layout of struct cp2_frame.
		# Notice also that KCC and KDC are not preserved.
		#
		dla	$k0, kernel_cp2_exception_framep_ptr
		clc	$kr1c, $k0, 0($kdc)

		# store 0;1-25;26;31
		cgetdefault	$kr2c
		csc	$kr2c, $0,  0*CAP_SIZE($kr1c)
		csc	$c1 , $0,   1*CAP_SIZE($kr1c)
		csc	$c2 , $0,   2*CAP_SIZE($kr1c)
		csc	$c3 , $0,   3*CAP_SIZE($kr1c)
		csc	$c4 , $0,   4*CAP_SIZE($kr1c)
		csc	$c5 , $0,   5*CAP_SIZE($kr1c)
		csc	$c6 , $0,   6*CAP_SIZE($kr1c)
		csc	$c7 , $0,   7*CAP_SIZE($kr1c)
		csc	$c8 , $0,   8*CAP_SIZE($kr1c)
		csc	$c9 , $0,   9*CAP_SIZE($kr1c)
		csc	$c10, $0,  10*CAP_SIZE($kr1c)
		csc	$c11, $0,  11*CAP_SIZE($kr1c)
		csc	$c12, $0,  12*CAP_SIZE($kr1c)
		csc	$c13, $0,  13*CAP_SIZE($kr1c)
		csc	$c14, $0,  14*CAP_SIZE($kr1c)
		csc	$c15, $0,  15*CAP_SIZE($kr1c)
		csc	$c16, $0,  16*CAP_SIZE($kr1c)
		csc	$c17, $0,  17*CAP_SIZE($kr1c)
		csc	$c18, $0,  18*CAP_SIZE($kr1c)
		csc	$c19, $0,  19*CAP_SIZE($kr1c)
		csc	$c20, $0,  20*CAP_SIZE($kr1c)
		csc	$c21, $0,  21*CAP_SIZE($kr1c)
		csc	$c22, $0,  22*CAP_SIZE($kr1c)
		csc	$c23, $0,  23*CAP_SIZE($kr1c)
		csc	$c24, $0,  24*CAP_SIZE($kr1c)
		csc	$c25, $0,  25*CAP_SIZE($kr1c)
		csc	$c26, $0,  26*CAP_SIZE($kr1c)
		csc	$c31, $0,  27*CAP_SIZE($kr1c)

		ccleartag	$kr2c, $kr2c # make sure we only use capabilities-based instructions
		csetdefault	$kr2c

		#
		# Context switch save; step all over k0 and k1 as is usual
		# for MIPS exception handlers.  Notice correspondence to the
		# layout of struct mips_frame.
		#
		dla	$k0, kernel_exception_framep_ptr
		clc	$kr1c, $k0, 0($kdc)

		csd	$at, $0,  0*REG_SIZE($kr1c)
		csd	$v0, $0,  1*REG_SIZE($kr1c)
		csd	$v1, $0,  2*REG_SIZE($kr1c)
		csd	$a0, $0,  3*REG_SIZE($kr1c)
		csd	$a1, $0,  4*REG_SIZE($kr1c)
		csd	$a2, $0,  5*REG_SIZE($kr1c)
		csd	$a3, $0,  6*REG_SIZE($kr1c)
		csd	$a4, $0,  7*REG_SIZE($kr1c)
		csd	$a5, $0,  8*REG_SIZE($kr1c)
		csd	$a6, $0,  9*REG_SIZE($kr1c)
		csd	$a7, $0, 10*REG_SIZE($kr1c)
		csd	$t0, $0, 11*REG_SIZE($kr1c)
		csd	$t1, $0, 12*REG_SIZE($kr1c)
		csd	$t2, $0, 13*REG_SIZE($kr1c)
		csd	$t3, $0, 14*REG_SIZE($kr1c)
		csd	$s0, $0, 15*REG_SIZE($kr1c)
		csd	$s1, $0, 16*REG_SIZE($kr1c)
		csd	$s2, $0, 17*REG_SIZE($kr1c)
		csd	$s3, $0, 18*REG_SIZE($kr1c)
		csd	$s4, $0, 19*REG_SIZE($kr1c)
		csd	$s5, $0, 20*REG_SIZE($kr1c)
		csd	$s6, $0, 21*REG_SIZE($kr1c)
		csd	$s7, $0, 22*REG_SIZE($kr1c)
		csd	$t8, $0, 23*REG_SIZE($kr1c)
		csd	$t9, $0, 24*REG_SIZE($kr1c)
		csd	$gp, $0, 25*REG_SIZE($kr1c)
		csd	$sp, $0, 26*REG_SIZE($kr1c)
		csd	$fp, $0, 27*REG_SIZE($kr1c)
		csd	$ra, $0, 28*REG_SIZE($kr1c)
		mfhi	$k1
		csd	$k1, $0, 29*REG_SIZE($kr1c)
		mflo	$k1
		csd	$k1, $0, 30*REG_SIZE($kr1c)
		dmfc0	$k1, $14		# EPC
		csd	$k1, $0, 31*REG_SIZE($kr1c)

		# Set up exception stack
		# todo:use real stack
		dla		$t0, __start_stack;
		dla		$t1, 0x80000
		csetoffset	$c11, $c30, $t0
		csetbounds	$c11, $c11, $t1
		move		$sp, $t1

		#
		# Restore the kernel's C0 after returning from the user
		# context -- this is the only capability used in the kernel
		# context other than KCC/KDC and T0 (for now).  In the future,
		# once the kernel uses capabilities in more intricate ways,
		# we'll need to restore other kernel capability context here.
		#
		csetdefault	$kdc

		#
		# Normal function invocation of kernel_exception().
		#
		cgetpcc		$c12
		dla		$t0, kernel_exception
		csetoffset	$c12, $c12, $t0
		cjalr		$c17, $c12
		nop

		#
		# Restore context from exception frame, which may have had
		# its contents replaced by the kernel.  Notice correspondence
		# to the layout of struct mips_frame.
		#
		dla	$k0, kernel_exception_framep_ptr
		clc	$kr1c, $k0, 0($kdc)

		cld	$at, $0,  0*REG_SIZE($kr1c)
		cld	$v0, $0,  1*REG_SIZE($kr1c)
		cld	$v1, $0,  2*REG_SIZE($kr1c)
		cld	$a0, $0,  3*REG_SIZE($kr1c)
		cld	$a1, $0,  4*REG_SIZE($kr1c)
		cld	$a2, $0,  5*REG_SIZE($kr1c)
		cld	$a3, $0,  6*REG_SIZE($kr1c)
		cld	$a4, $0,  7*REG_SIZE($kr1c)
		cld	$a5, $0,  8*REG_SIZE($kr1c)
		cld	$a6, $0,  9*REG_SIZE($kr1c)
		cld	$a7, $0, 10*REG_SIZE($kr1c)
		cld	$t0, $0, 11*REG_SIZE($kr1c)
		cld	$t1, $0, 12*REG_SIZE($kr1c)
		cld	$t2, $0, 13*REG_SIZE($kr1c)
		cld	$t3, $0, 14*REG_SIZE($kr1c)
		cld	$s0, $0, 15*REG_SIZE($kr1c)
		cld	$s1, $0, 16*REG_SIZE($kr1c)
		cld	$s2, $0, 17*REG_SIZE($kr1c)
		cld	$s3, $0, 18*REG_SIZE($kr1c)
		cld	$s4, $0, 19*REG_SIZE($kr1c)
		cld	$s5, $0, 20*REG_SIZE($kr1c)
		cld	$s6, $0, 21*REG_SIZE($kr1c)
		cld	$s7, $0, 22*REG_SIZE($kr1c)
		cld	$t8, $0, 23*REG_SIZE($kr1c)
		cld	$t9, $0, 24*REG_SIZE($kr1c)
		cld	$gp, $0, 25*REG_SIZE($kr1c)
		cld	$sp, $0, 26*REG_SIZE($kr1c)
		cld	$fp, $0, 27*REG_SIZE($kr1c)
		cld	$ra, $0, 28*REG_SIZE($kr1c)
		cld	$k1, $0, 29*REG_SIZE($kr1c)
		mthi	$k1
		cld	$k1, $0, 30*REG_SIZE($kr1c)
		mtlo	$k1
		cld	$k1, $0, 31*REG_SIZE($kr1c)
		dmtc0	$k1, $14		# EPC

		#
		# Restore the capability exception frame -- after
		# general-purpose registers, since the above code is done
		# relative to $c0.  Note that, after this point, no further
		# memory access can be done using MIPS load and store
		# instructions.
		#
		dla	$k0, kernel_cp2_exception_framep_ptr
		clc	$kr1c, $k0, 0($kdc)

		# load 0;1-25;26;31
		clc	$kr2c, $0,  0*CAP_SIZE($kr1c)
		csetdefault	$kr2c
		clc	$c1 , $0,   1*CAP_SIZE($kr1c)
		clc	$c2 , $0,   2*CAP_SIZE($kr1c)
		clc	$c3 , $0,   3*CAP_SIZE($kr1c)
		clc	$c4 , $0,   4*CAP_SIZE($kr1c)
		clc	$c5 , $0,   5*CAP_SIZE($kr1c)
		clc	$c6 , $0,   6*CAP_SIZE($kr1c)
		clc	$c7 , $0,   7*CAP_SIZE($kr1c)
		clc	$c8 , $0,   8*CAP_SIZE($kr1c)
		clc	$c9 , $0,   9*CAP_SIZE($kr1c)
		clc	$c10, $0,  10*CAP_SIZE($kr1c)
		clc	$c11, $0,  11*CAP_SIZE($kr1c)
		clc	$c12, $0,  12*CAP_SIZE($kr1c)
		clc	$c13, $0,  13*CAP_SIZE($kr1c)
		clc	$c14, $0,  14*CAP_SIZE($kr1c)
		clc	$c15, $0,  15*CAP_SIZE($kr1c)
		clc	$c16, $0,  16*CAP_SIZE($kr1c)
		clc	$c17, $0,  17*CAP_SIZE($kr1c)
		clc	$c18, $0,  18*CAP_SIZE($kr1c)
		clc	$c19, $0,  19*CAP_SIZE($kr1c)
		clc	$c20, $0,  20*CAP_SIZE($kr1c)
		clc	$c21, $0,  21*CAP_SIZE($kr1c)
		clc	$c22, $0,  22*CAP_SIZE($kr1c)
		clc	$c23, $0,  23*CAP_SIZE($kr1c)
		clc	$c24, $0,  24*CAP_SIZE($kr1c)
		clc	$c25, $0,  25*CAP_SIZE($kr1c)
		clc	$c26, $0,  26*CAP_SIZE($kr1c)
		clc	$c31, $0,  27*CAP_SIZE($kr1c)

		#
		# On general principle, flush the contents of $k0 and $k1
		# before returning to "userspace".
		#
		move	$k0, $zero
		move	$k1, $zero

		eret
		.end kernel_cp2_exception_handler
