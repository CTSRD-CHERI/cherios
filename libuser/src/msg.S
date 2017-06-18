#-
# Copyright (c) 2016 Hongyan Xia
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

.set REG_SIZE_BITS, 3
.set MUTEX_ADDR, 0x100

# Dispatch CCalls

	.text
	.global msg_entry
	.ent msg_entry
msg_entry:
    li $t9, MUTEX_ADDR
    dadd    $t9, $s4, $t9
mutex_loop:
    # mutex to prevent module from receiving another message while still
    # processing the previous one
    ll  $t1, 0($t9)
    bne $t1, $0, locked
    li  $t1, 1
    sc  $t1, 0($t9)
    beq $t1, $0, locked
    nop

	#Save sync_token
	dla		$t9, sync_token
	dadd    $t9, $s4, $t9
	sd		$t0, 0($t9)

	bltz		$v0, control
	nop

	dla		$t0, msg_methods
	dadd    $t0, $s4, $t0
	dla		$t1, msg_methods_nb
	dadd    $t1, $s4, $t1
	b		call
	nop

control:
	dla		$t0, ctrl_methods
	dadd    $t0, $s4, $t0
	dla		$t1, ctrl_methods_nb
    dadd    $t1, $s4, $t1
	sub		$v0, $0, $v0
call:
	ld		$t1, 0($t1)
	sub		$t1, $t1, $v0
	blez		$t1, overflow
	nop

	sll		$v0, $v0, REG_SIZE_BITS
	add		$t0, $t0, $v0
	ld		$t9, 0($t0)
    dadd    $t9, $s4, $t9
	jalr    $t9
	nop
	b		end
	nop

overflow:
	move		$v0, $0
	b		end
	move		$v1, $0

end:
	#Restore sync_token
	dla		$t1, sync_token
	dadd    $t1, $s4, $t1
	ld		$t0, 0($t1)
    li $t1, MUTEX_ADDR
    dadd    $t1, $s4, $t1
    sw  $0, 0($t1)
    li $v1, 1010
    syscall

	#We are scheduled again: new message
	b msg_entry
	nop

locked:
    li  $v1, 13
    move    $s0, $a0
    li  $a0, 0
    syscall
    b   mutex_loop
    move    $a0, $s0

msg_entry_end:
	.end msg_entry
	.size msg_entry, msg_entry_end - msg_entry

	.text
	.global msg_entry_dcall
	.ent msg_entry_dcall
msg_entry_dcall:
    li $t9, MUTEX_ADDR
    dadd    $t9, $s4, $t9
mutex_loop_dcall:
    # mutex to prevent module from receiving another message while still
    # processing the previous one
    ll  $t1, 0($t9)
    bne $t1, $0, locked_dcall
    li  $t1, 1
    sc  $t1, 0($t9)
    beq $t1, $0, locked_dcall
    nop

    dla     $t1, act_self_stack
    dadd    $t1, $s4, $t1
    ld      $sp, 0($t1)
    daddiu  $sp, $sp, -16
    sd      $t0, 8($sp) # store the caller's base here

	bltz		$v0, control_dcall

	dla		$t0, msg_methods
	dadd    $t0, $s4, $t0
	dla		$t1, msg_methods_nb
	dadd    $t1, $s4, $t1
    b   call_dcall
	nop

control_dcall:
	dla		$t0, ctrl_methods
	dadd    $t0, $s4, $t0
	dla		$t1, ctrl_methods_nb
    dadd    $t1, $s4, $t1
	sub		$v0, $0, $v0
call_dcall:
	ld		$t1, 0($t1)
	sub		$t1, $t1, $v0
	blez		$t1, overflow_dcall
	nop

    sd  $ra, 0($sp)
	sll		$v0, $v0, REG_SIZE_BITS
	add		$t0, $t0, $v0
	ld		$t9, 0($t0)
    dadd    $t9, $s4, $t9
	jalr    $t9
	nop
    ld  $ra, 0($sp)
    b   end_dcall
	nop

overflow_dcall:
	move		$v0, $0
    b   end_dcall
	move		$v1, $0

end_dcall:
    # release lock
    li $t0, MUTEX_ADDR
    dadd    $t0, $s4, $t0
    sw  $0, 0($t0)
    ld      $s4, 8($sp)
    jr      $ra
    daddiu  $sp, $sp, 16

locked_dcall:
    li  $v1, 13
    move    $s0, $a0
    li  $a0, 0
    syscall
    b   mutex_loop_dcall
    move    $a0, $s0

msg_entry_dcall_end:
	.end msg_entry_dcall
	.size msg_entry_dcall, msg_entry_dcall_end - msg_entry_dcall
