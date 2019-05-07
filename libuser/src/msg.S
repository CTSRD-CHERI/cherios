#-
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

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
#include "asm.S"
#include "queue.h"
#include "syscalls.h"
#include "mips.h"

# We have so many registers, we may as well use a few rather than using get_sym all the time

#define tmp_cap             $c13

#define queue               $c20
#define self_ctrl_cap       $c21
#define sync_state_cap      $c22
#define msg_table           $c23
#define ctrl_table          $c24

#define msg_methods_nb_g    $s0
#define ctrl_methods_nb_g   $s1

#define start_g             $s2
#define timeout             $s3
#define flags               $s4

#define STACK_LAYOUT 7 \
    $c17, $c18, queue, self_ctrl_cap, sync_state_cap, msg_table, ctrl_table, \
    msg_methods_nb_g, ctrl_methods_nb_g, start_g, timeout, flags

# Message read loop

	.text
	.global msg_entry
	.ent msg_entry

# Load a bunch of symbols using the cap_table

.type act_self_queue, "tls_object"
.type sync_state, "tls_object"
.type act_self_ctrl, "tls_object"

.weak msg_methods
.weak ctrl_methods
.weak msg_methods_nb
.weak ctrl_methods_nb

.global msg_entry
msg_entry:

    FRAME_PUSH  STACK_LAYOUT

    move        timeout, $a0
    move        flags, $a1

    get_tls_sym_cap queue, act_self_queue
    get_tls_sym sync_state_cap, sync_state

    get_sym_val msg_methods_nb_g, msg_methods_nb, d, tmp_cap
    get_sym_val ctrl_methods_nb_g, ctrl_methods_nb, d, tmp_cap
    get_sym_val ctrl_methods_nb_g, ctrl_methods_nb, d, tmp_cap

    get_sym     msg_table, msg_methods
    get_sym     ctrl_table, ctrl_methods

    get_tls_sym_val self_ctrl_cap, act_self_ctrl, c, tmp_cap

    li          $a0, 0
    li          $a1, 1
    cnull       $c3
    cnull       $c4
msg_entry_loop:

# Check for items in queue
    clw     start_g, $zero, HEADER_START_OFFSET(queue)    # load start
    clc     tmp_cap, $zero, HEADER_END_OFFSET(queue)
    clw     $t1, $zero, 0(tmp_cap)                        # $t1 = *end
    bne     start_g, $t1, pop                             # start_g != $t1
    nop

    beqz    timeout, return_to_caller


    andi    $a3, flags, 1                                # MSG_ENTRY_TIMEOUT_ON_NOTIFY
    daddiu  $a2, timeout, 1                              # kernel thinks 0 is inf, not -1. Just add 1.

# TODO in a multicore world we may wish to spin a few times before resorting to WAIT
# fastpath_wait (optionally) combines a return, a queue wait, and a pop
# a0/a1/c3 are return args. c4 a token (NULL if no return). a4 is timeout. a5 is whether notify counts
    call_func fastpath_wait
# v1 will contain an enum:
    # 0 is fastpath achieved, all registers are already set up
    # 1 is timeout
    # 2 is we need to pop our own message, but no timeout occured

    beqz    $v1, handle_message
    daddiu  $v1, $v1, -1
    cnull   $c4
    beqz    $v1, msg_entry_loop
    movz    timeout, $zero, $v1                          # if a timeout occurs we still might check messages
    b       pop_2                                        # but set timeout to 0 so when messages are finished we return
    nop

pop:
    # we might be holding on to a return value. Send it without a switch
    cbtu    $c4, pop_2
    li      $a2, 0 # don't hint switch if we are about to pop a message
    call_func message_reply

pop_2:
    clw      $t1, $zero, HEADER_LEN_OFFSET(queue)          # load length
    daddiu   $t1, $t1, -1
    and      $t1, $t1, start_g
    dsll     $t1, $t1, (MSG_LEN_SHIFT)                     # t1 has offet of message in queue

    # pops message #
    clc     $c3, $t1,  (MSGS_START_OFFSET)(queue)
    clc     $c4, $t1,  (MSGS_START_OFFSET + CAP_SIZE)(queue)
    clc     $c5, $t1,  (MSGS_START_OFFSET + 2 * CAP_SIZE)(queue)
    clc     $c6, $t1,  (MSGS_START_OFFSET + 3 * CAP_SIZE)(queue)
    clc     $c1, $t1,  (MSGS_START_OFFSET + 4 * CAP_SIZE)(queue)
    cld     $a0, $t1,  (MSGS_START_OFFSET + 5 * CAP_SIZE)(queue)
    cld     $a1, $t1,  (MSGS_START_OFFSET + (5 * CAP_SIZE) + 8)(queue)
    cld     $a2, $t1,  (MSGS_START_OFFSET + (5 * CAP_SIZE) + 16)(queue)
    cld     $a3, $t1,  (MSGS_START_OFFSET + (5 * CAP_SIZE) + 24)(queue)
    cld     $v0, $t1,  (MSGS_START_OFFSET + (5 * CAP_SIZE) + 32)(queue)


    addi    start_g, start_g, 1                             # increment start

update_start:
    csw     start_g, $zero, HEADER_START_OFFSET(queue)  # save start

handle_message:

    andi    $t0, flags, 2
    movn    timeout, $zero, $t0

    cmove   $c12, msg_table
	bgez	$v0, call
    move    $t1, msg_methods_nb_g

control:
    cmove   $c12, ctrl_table
	move    $t1, ctrl_methods_nb_g
	sub		$v0, $0, $v0
call:
	sltu    $t1, $v0, $t1
	beqz    $t1, overflow
	sll		$v0, $v0, CAP_SIZE_BITS

	#Save sync_token
	csc		$c1, $zero, 0(sync_state_cap)

	clc		$c12, $v0, 0($c12)
	cjalr		$c12, $c17
	cmove   $c18, $idc

	#Restore sync_token. Args already set properly

	clc		$c4, $zero, 0(sync_state_cap)
    move    $a0, $v0
    b msg_entry_loop
    move    $a1, $v1

overflow:
    cmove       $c4, $c1
	move		$v0, $0
	move		$v1, $0
	b		    msg_entry_loop
	cnull       $c3
end:

return_to_caller:

# must not forget return if we saved it
    cbtu    $c4, pop_frame
    li      $a2, 0 # don't hint switch if we are about to pop a message
    call_func message_reply

pop_frame:
    POP_AND_RETURN STACK_LAYOUT

msg_entry_end:
	.end msg_entry
	.size msg_entry, msg_entry_end - msg_entry
