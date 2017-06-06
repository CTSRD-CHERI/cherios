#-
# Copyright (c) 2016 Robert N. M. Watson
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

.set noreorder
.set nobopt
.set noat

.ifeq _MIPS_SZCAP-256
.set CAP_SIZE_BITS, 5
.set CAP_SIZE,     32
.set CAP_IN_MESSAGE, 7
.else
.ifeq _MIPS_SZCAP-128
.set CAP_SIZE_BITS, 4
.set CAP_SIZE,     16
.set CAP_IN_MESSAGE, 6
.else
.error "Unknown cap size"
.endif
.endif

#define __ASSEMBLY__ 1
#include "syscalls.h"

# Assembly stub on the way to running C code.  Assumes that $c0 and $pcc are
# global and suitable.  Jump to various startup functions to initialise the C
# runtime, and then call main().

.type act_self_ctrl, "tls_object"
.type msg_enable, "tls_object"

	.text
	.global start_common
	.ent start_common
start_common:

#########################################################################################################################
# Some "documentation" for the interface between the kernel and activation start                                        #
# These fields are setup by the caller of act_register                                                                  #
#                                                                                                                       #
# a0    : user GP argument (goes to main)                                                                               #
# c3    : user Cap argument (goes to main)                                                                              #
#                                                                                                                       #
# These fields are setup by act_register itself. Although the queue is an argument to the function                      #
#                                                                                                                       #
# c21   : self control reference                                                                                        #
# c22   : process reference                                                                                             *
# c23   : namespace reference (may be null for init and namespace)                                                      #
# c24   : kernel interface table                                                                                        #
# c25   : queue                                                                                                         #
#########################################################################################################################

	# Save arguments
	move		$s0, $a0
    cmove       $c20, $c3

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

	# Call object init.
	cmove		$c3, $c21 #self_ctrl
	cmove		$c4, $c23 #ns_ref
	cmove		$c5, $c24 #kernel_if
	cmove       $c6, $c25 #queue
	cmove       $c7, $c22 # process ref

	cgetpcc		$c12
	dla		$t0, libuser_init
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# Restore arguments
	move		$a0, $s0
	cmove		$c3, $c20

	# Call main().
	cgetpcc		$c12
	dla		$t0, main
	csetoffset	$c12, $c12, $t0
	cjalr		$c12, $c17
	nop

	# main() done. Go to msg_entry if msg_enable, call terminate otherwise
	rdhwr   $t1, $29
	daddiu  $t1, $t1, -0x7000
	dla		$t0, msg_enable
	daddu   $t0, $t0, $t1
	cgetdefault	$c17
	cld		$t0, $t0, 0($c17)
	bne		$t0, $zero, msg_entry
	nop
	dla		$t0, act_self_ctrl
	daddu   $t0, $t0, $t1
	clc		$c2, $t0, 0($c17)
	cmove   $c3, $c2
	dla     $t0, kernel_if
	clc     $c1, $t0, syscall_act_terminate_offset($c17)
	# We don't bother setting up a return as this will terminate us
	ccall   $c1, $c2