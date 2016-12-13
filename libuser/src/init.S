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
# global and suitable.  Jump to various startup functions to initialise the C
# runtime, and then call main().

	.text
	.global start_common
	.ent start_common
start_common:
	# Save arguments
	move		$s0, $a0
	move		$s1, $a1
	move		$s2, $a2
	move		$s3, $a3

	# Initialise BSS and globals before running C constructors, so that C
	# constructors can take them (and other aspects of the conventional C
	# runtime) for granted.

	# Zero BSS.
    dla     $t9, crt_init_bss
    dadd    $t9, $s4, $t9
    jalr    $t9
    nop

	# Initialise globals.
    dla     $t9, crt_init_globals
    dadd    $t9, $s4, $t9
    jalr    $t9
    nop

	# Call C constructors.
    dla     $t9, crt_call_constructors
    dadd    $t9, $s4, $t9
    jalr    $t9
    nop

	# Call object init.
	move		$a0, $s3
	move		$a1, $s5
	move		$a2, $s6
	move		$a3, $s7 # careful, s8 is used to remember pc for PIC
    dla     $t9, libuser_init
    dadd    $t9, $s4, $t9
    jalr    $t9
    nop

	# Restore arguments
	move		$a0, $s0
	move		$a1, $s1
	move		$a2, $s2

	# Call main().
    dla     $t9, main
    dadd    $t9, $s4, $t9
    jalr    $t9
    nop

	# main() done. Exit or CReturn
	dla		$t0, msg_enable
	dadd		$t0, $s4
    #cgetdefault	$c17
    #cld		$t0, $t0, 0($c17)
    ld  $t0, 0($t0)
	bne		$t0, $zero, to_msg
	nop
	dla		$t0, act_self_ctrl
	dadd		$t0, $s4
    ld  $a0, 0($t0)
    #clc		$c3, $t0, 0($c17)
	li		$v1, 26
	syscall
    nop

to_msg:
	# CReturn to go in waiting state.
	move    $v0, $0
#jr  $ra
    li $v1, 1010
    syscall
    nop

	#First 'real' message. Go to appropriate handler
#j msg_entry
#nop
    dla $t9, msg_entry
    dadd $t9, $s4, $t9
    jr  $t9
    nop
