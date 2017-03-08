#-
# Copyright (c) 2011 Robert N. M. Watson
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

.set MIPS_SZCAP, _MIPS_SZCAP
.include "asm.S"

#define __ASSEMBLY__ 1
#include "critical.h"

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

#set bad instruction global as QEMU does not support it
        clw $k0, $zero, 0($epcc)
        dla $k1, badinstr_glob
        csd $k0, $k1, 0($kdc)

#cancel if we are in critical section. We assume that critical code never throws exceptions and only async interrupts happen.
        dla $k0, critical_state
        cld $k1, $k0, CRIT_STATE_LEVEL_OFFSET($kdc)
        beqz $k1, take_exception
        nop

skip_exception:
        dmfc0   $k1, $13
        csd     $k1, $k0, CRIT_STATE_CAUSE_OFFSET($kdc)
        dmfc0   $k1, $12
        daddiu  $k1, -1     # As interrupts had to be enabled
        mtc0    $k1, $12    # Disable interrupts, we are happy to take the hit as this is rare
        move	$k0, $zero
        move	$k1, $zero
        eret

take_exception:


		#
		# Since we are still running with user capabilities in most
		# capability registers, we must access all kernel data
		# relative to KDC.  First save and clear CP2 context, then
		# move on to general-purpose registers.
		#
		dla	$k0, kernel_exception_framep_ptr
		clc	$kr1c, $k0, 0($kdc)

        cgetdefault	$kr2c
        save_reg_frame $kr1c, $k1, $kr2c, $epcc

		# Set up exception stack
		dla		$t0, __start_exception_stack
		dla		$t1, __size_exception_stack
		csetoffset	$c11, $kdc, $t0
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
		cjalr		$c12, $c17
		nop

.global kernel_exception_restore
kernel_exception_restore:

        dla	$k0, kernel_exception_framep_ptr
        clc	$kr1c, $k0, 0($kdc)

        restore_reg_frame $kr1c, $k1, $kr2c, $epcc
        csetdefault	$kr2c

        # Counter eret bug
        cgetoffset $k0, $epcc
        dmtc0      $k0, $14

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
