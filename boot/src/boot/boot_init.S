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

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
#include "asm.S"
#include "mips.h"
#include "assembly_utils.h"
#include "macroutils.h"

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

    # Set user interrupt bits
    or      $t0, $t0, (0x3 << 8)

    mtc0		$t0, $12

	# Wait for capability coprocessor to be enabled for real
	nop; nop; nop; nop; nop; nop; nop; nop;

	# Clear other capability registers to ensure we don't retain any
	# executable capabilities: everything but $pcc and $c0.

# In the current QEMU register 0 is $ddc, in hardware it is $c0 still
#ifdef HARDWARE_qemu
	CClearLo	0xfffe
#else
    CClearLo	0xffff
#endif
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

#ifndef CHERI_LEGACY_COMPAT
    dmtc0  $zero, MIPS_CP0_REG_REVOKE, MIPS_CP0_REG_REVOKE_PERMS
    dmtc0  $zero, MIPS_CP0_REG_REVOKE, MIPS_CP0_REG_REVOKE_BASE
    dmtc0  $zero, MIPS_CP0_REG_REVOKE, MIPS_CP0_REG_REVOKE_BOUND
#endif

    # For BERI we need to enable the larger TLB first

#ifndef HARDWARE_qemu
    mfc0    $t0, MIPS_CP0_REG_CONFIG, 6
    li      $t1, (1 << 2)
    or      $t0, $t0, $t1
    mtc0    $t0, MIPS_CP0_REG_CONFIG, 6
#endif

    # Setup the TLB so it won't break on first miss
    invalidate_tlb  $t0, $t1

#ifdef SMP_ENABLED

    #FIXME this is for QEMU not FPGA
	# Send SMP cores other than 0 to trap

    dmfc0   $a0, $15, 1
    andi    $a0, $a0, 0xff
    bnez    $a0, bootload_smp_trap
    nop

    # Enter config mode

    mfc0    $t2, MIPS_CP0_REG_MVPControl
    ori     $t2, $t2, MVPControl_VPC
    dli     $t1, ~MVPControl_EVP
    and     $t2, $t2, $t1
    mtc0    $t2, MIPS_CP0_REG_MVPControl

    li      $t0, 1
1:
    SET_VPE_CONTROL_TargTC $t1, $t0

    # Activate Thread
    MFTC0(t1, MIPS_CP0_REG_TCStatusX)
    li      $t3, TCStatus_A
    or      $t1, $t1, $t3
    MTTC0(t1, MIPS_CP0_REG_TCStatusX)

    # Unhalt Thread
    MFTC0(t1, MIPS_CP0_REG_TCHaltX)
    MTTC0(zero, MIPS_CP0_REG_TCHaltX)

    # Activate VPE
    MFTC0(t1, MIPS_CP0_REG_VPEConf0X)
    ori     $t1, $t1, VPEConf0_MVP | VPEConf0_VPA
    MTTC0(t1, MIPS_CP0_REG_VPEConf0X)

    daddiu  $t0, $t0, -1
    bnez     $t0, 1b
    nop

    # Now exit config

    xori     $t2, $t2, MVPControl_VPC | MVPControl_EVP
    mtc0    $t2, MIPS_CP0_REG_MVPControl

    # And enable things

    EVPE
#endif

	# Set up a boot-time stack using memory allocated by the linker;
	# assume that $c0 is a sensible starting capability for this.
	dla		$t0, __start_boot_stack
	dla		$t1, __size_boot_stack
	cgetdefault	$c11
	csetoffset	$c11, $c11, $t0
	csetbounds	$c11, $c11, $t1
	csetoffset  $c11, $c11, $t1

	# Initialise BSS and globals before running C constructors, so that C
	# constructors can take them (and other aspects of the conventional C
	# runtime) for granted.

	# Initialise globals. We need to use dla to call this function.
	cgetpcc		$c12
	dla		$t0, crt_init_globals_boot
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	cmove   $c18, $c11

    cmove   $idc, $c3
	# Zero BSS.
	call_func crt_init_boot_bss

	# Call C constructors.
	call_func crt_call_constructors

	# Call bootloader_main().
	cgetpccsetoffset $c3, $zero                     # Need this to operate a tmp alloc pool
	call_func bootloader_main

#ifdef SMP_ENABLED
	# Trap for threads other than man
bootload_smp_trap:

    # a0 contains our CPU ID

    # A vector of characters to signal when to exit this trap. 0 = spin. 1 = try exit. 2 = try exit
    dla $t0, smp_signal_vector
    daddu   $t0, $t0, $a0

    # A vector of capabilities for when to go on exit
    dla $t1, smp_destination_vector
    dsll $a0, $a0, CAP_SIZE_BITS
    daddu  $t1, $t1, $a0


    # Spin until signal non zero
bootload_smp_spin:
    YIELD
    nop
    clb $t2, $t0, 0($c3)
    beqz $t2, bootload_smp_spin
    nop

    # Load destination, and jump out while setting signal to 2
    li  $t2, 2
    clc $c4, $t1, 0($c3)
    cjr $c4
    csb $t2, $t0, 0($c3)
#endif