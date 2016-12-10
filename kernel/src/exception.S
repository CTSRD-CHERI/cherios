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

.set MIPS_SZCAP, _MIPS_SZCAP
.include "asm.S"

#
# Capability-aware exception handler.
#
# Code in this file assumes that the kernel executes in a single global
# memory segment that will be described by KCC (and installed in PCC during
# entry to the exception vector) and KDC.  It is possible to imagine linking
# the kernel so that KCC describes only code, and KDC describes only data, but
# we don't yet do this.
#
# Exception code can safely use $k0, $k1, $kr1c, $kr2c, $kcc and $kdc to
# perform its context switch.  PC and PCC are both implicitly used, with
# pre-exception values preserved in EPC and EPCC respectively.
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
        dla	$k0, kernel_exception_framep_ptr
        ld  $k0, 0($k0)
        cgetdefault $kr1c
        csetoffset $kr1c, $kr1c, $k0
        dla	$k0, 32*CAP_SIZE + 32*REG_SIZE #Should be enough
        csetbounds $kr1c, $kr1c, $k0

		# Notice correspondence to the layout of struct reg_frame.
		# Notice also that KCC and KDC are not preserved.
		#
		cgetdefault	$kr2c
		# Expands to a list of the form
		# csc	$creg, $0, offset*CAP_SIZE+MIPS_FRAME_SIZE($kr1c)
        #foreachi csave, 0, $kr1c,					\
		#	$kr2c, $c1, $c2, $c3, $c4, $c5, $c6, $c7, $c8, $c9,	\
		#	$c10, $c11, $c12, $c13, $c14, $c15, $c16, $c17, $c18,	\
		#	$c19, $c20, $c21, $c22, $c23, $c24, $c25, $c26, $c31

		#
		# Context switch save; step all over k0 and k1 as is usual
		# for MIPS exception handlers.  Notice correspondence to the
		# layout of struct reg_frame.
		#
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

		# Set up exception stack
		dla		$t1, __stop_exception_stack
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
		dla		$t9, kernel_exception
		jalr		$t9
		nop

		#
		# Restore context from exception frame, which may have had
		# its contents replaced by the kernel.  Notice correspondence
		# to the layout of struct reg_frame.
		#
        dla	$k0, kernel_exception_framep_ptr
        ld  $k0, 0($k0)
        cgetdefault $kr1c
        csetoffset $kr1c, $kr1c, $k0
        dla	$k0, 32*CAP_SIZE + 32*REG_SIZE #Should be enough
        csetbounds $kr1c, $kr1c, $k0

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

		#
		# Restore the capability exception frame -- after
		# general-purpose registers, since the above code is done
		# relative to $c0.  Note that, after this point, no further
		# memory access can be done using MIPS load and store
		# instructions.
		# Notice correspondence to the layout of struct reg_frame.
		#
		# Expands to a list of the form
		# clc	$creg, $0, offset*CAP_SIZE+MIPS_FRAME_SIZE($kr1c)
        #foreachi crestore, 0, $kr1c,					\
		#	$kr2c, $c1, $c2, $c3, $c4, $c5, $c6, $c7, $c8, $c9,	\
		#	$c10, $c11, $c12, $c13, $c14, $c15, $c16, $c17, $c18,	\
		#	$c19, $c20, $c21, $c22, $c23, $c24, $c25, $c26, $c31
		#csetdefault	$kr2c
		cgetdefault $c31

		#
		# On general principle, flush the contents of $k0 and $k1
		# before returning to "userspace".
		#
		move	$k0, $zero
		move	$k1, $zero

		eret

kernel_cp2_exception_handler_end:
		.size kernel_cp2_exception_handler, kernel_cp2_exception_handler_end - kernel_cp2_exception_handler
		.end kernel_cp2_exception_handler
