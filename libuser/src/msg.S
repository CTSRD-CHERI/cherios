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
#define return              $s3

#define STACK_LAYOUT 7 \
    $c17, $c18, queue, self_ctrl_cap, sync_state_cap, msg_table, ctrl_table, \
    msg_methods_nb_g, ctrl_methods_nb_g, start_g, return

# Message read loop

	.text
	.global msg_entry
	.ent msg_entry

# Load a bunch of symbols using the cap_table

.type act_self_queue, "tls_object"
.type sync_state, "tls_object"
.type act_self_ctrl, "tls_object"

.global msg_entry
msg_entry:

    FRAME_PUSH  STACK_LAYOUT

    move        return, $a0

    get_tls_sym_cap queue, act_self_queue
    get_tls_sym sync_state_cap, sync_state

    get_sym_val msg_methods_nb_g, msg_methods_nb, d, tmp_cap
    get_sym_val ctrl_methods_nb_g, ctrl_methods_nb, d, tmp_cap
    get_sym_val ctrl_methods_nb_g, ctrl_methods_nb, d, tmp_cap

    get_sym     msg_table, msg_methods
    get_sym     ctrl_table, ctrl_methods

    get_tls_sym_val self_ctrl_cap, act_self_ctrl, c, tmp_cap

msg_entry_loop:

# Check for items in queue
    clw     start_g, $zero, HEADER_START_OFFSET(queue)    # load start
    clc     tmp_cap, $zero, HEADER_END_OFFSET(queue)
    clw     $t1, $zero, 0(tmp_cap)                        # $t1 = *end
    bne     start_g, $t1, pop                             # start_g != $t1
    nop

    bnez    return, return_to_caller
# TODO in a multicore world we may wish to spin a few times before resorting to WAIT
    call_func wait

pop:
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
	bltz	$v0, control
	nop

    cmove   $c12, msg_table
	move    $t1, msg_methods_nb_g
	b		call
	nop

control:
    cmove   $c12, ctrl_table
	move    $t1, ctrl_methods_nb_g
	sub		$v0, $0, $v0
call:
	sub		$t1, $t1, $v0
	blez		$t1, overflow
	nop

	#Save sync_token
	csc		$c1, $zero, 0(sync_state_cap)

	sll		$v0, $v0, CAP_SIZE_BITS
	clc		$c12, $v0, 0($c12)
	cjalr		$c12, $c17
	cmove   $c18, $idc

	b		end
	nop

overflow:
	move		$v0, $0
	move		$v1, $0
	cfromptr	$c3, $c3, $0
	b		end
	nop

end:

	#Restore sync_token
	clc		$c4, $zero, 0(sync_state_cap)

#FIXME this instruction cannot jump backwards due to a QEMU bug
	cbts    $c4, make_return
	nop
    b msg_entry_loop
	nop

make_return:
    # c3 is both a return register and an arugment register so needs no work.
    # v0 and v1 need to be arguments.

    # c1 needs the return cap, we kept it in a saved register
    # c2 needs our self ctrl
    # c4 needs the caller (from above)
    # c5 needs the sync (from above)
    # c17 needs a good return address. We took the address of the loop symbol which is as good as any
    move  $a0, $v0
    move  $a1, $v1
    call_func message_reply

    b msg_entry_loop
    nop

return_to_caller:
    POP_AND_RETURN STACK_LAYOUT

msg_entry_end:
	.end msg_entry
	.size msg_entry, msg_entry_end - msg_entry
