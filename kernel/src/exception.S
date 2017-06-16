#-
# Copyright (c) 2016 Hongyan Xia
# Copyright (c) 2016 Hadrien Barral
# Copyright (c) 2011 Robert N. M. Watson
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

        dla	$k0, kernel_exception_framep_ptr
        ld  $k0, 0($k0)

		#
		# Context switch save; step all over k0 and k1 as is usual
		# for MIPS exception handlers.  Notice correspondence to the
		# layout of struct reg_frame.
		#
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

		# Set up exception stack
		dla		$sp, __stop_exception_stack

		#
		# Restore the kernel's C0 after returning from the user
		# context -- this is the only capability used in the kernel
		# context other than KCC/KDC and T0 (for now).  In the future,
		# once the kernel uses capabilities in more intricate ways,
		# we'll need to restore other kernel capability context here.
		#

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
