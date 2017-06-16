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
.set REG_SIZE, (64/8)

# Assembly stub on the way to running C code.  Assumes that $c0 and $pcc are
# global and suitable.  Enable capability coprocessor.  Set up a stack.  Jump
# to various startup functions to initialise the C runtime, and then call
# cherios_main().

	.text
	.global start
	.ent start
start:
    
    dla $t9, __init_fs_start
    sd  $t0, 0($t9)
    dla $t9, __init_fs_stop
    sd  $t1, 0($t9)

	# Jump to write-free $pcc.
	dla		$t0, cherios_init_jump_target
	jr		$t0
	nop
cherios_init_jump_target:
	# Set up a stack using memory allocated by the linker;
	# assume that $c0 is a sensible starting capability for this.
	dla		$t0, __stop_init_stack
	move		$sp, $t0

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

	# On the off chance that cherios_main() has been defined as having
	# arguments, set the first argument (argc) to 0.
	dli		$a0, 0

	# Call init_main().
	dla		$t9, init_main
    jalr    $t9
	nop

	# Bootloader done. Go in waiting state. #fixme: revoke object
    move    $v0, $0
#jr  $ra
    li $v1, 1010
    syscall
    nop

    .data
    .global __init_fs_start
    .global __init_fs_stop

__init_fs_start:
    .skip REG_SIZE, 0
__init_fs_stop:
    .skip REG_SIZE, 0
