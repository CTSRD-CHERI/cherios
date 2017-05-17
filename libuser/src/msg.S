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

#define __ASSEMBLY__ 1
#include "queue.h"
#include "syscalls.h"

.set noreorder
.set nobopt
.set noat

.ifeq _MIPS_SZCAP-256
.set CAP_SIZE_BITS, 5
.set CAP_SIZE,     32
.set CAP_IN_MESSAGE, 14
.else
.ifeq _MIPS_SZCAP-128
.set CAP_SIZE_BITS, 4
.set CAP_SIZE,     16
.set CAP_IN_MESSAGE, 12
.else
.error "Unknown cap size"
.endif
.endif

#define message_reply       $c19
#define msg_entry_loop_cap  $c20
#define def_cap             $c21
#define queue               $c22

# These are used by the runtime to know who to respond to
# FIXME should be local, anybody who wants to use creturn should do so to a creturn method
	.data
	.global sync_caller
	.global sync_token
	.size sync_caller, CAP_SIZE
	.size sync_token, CAP_SIZE
	.align CAP_SIZE_BITS
sync_caller:
	.space CAP_SIZE
sync_token:
	.space CAP_SIZE

# Message read loop

	.text
	.global msg_entry
	.ent msg_entry
msg_entry:
    cgetdefault	def_cap
    dla         $t0, act_self_queue
    clc         queue, $t0, 0(def_cap)
    dla         $t0, kernel_if
    clc         message_reply, $t0, CAP_SIZE(def_cap)
    cgetpcc     msg_entry_loop_cap

msg_entry_loop:

# Check for items in queue
    cld     $t0, $zero, HEADER_START_OFFSET (queue)       # $t0 = start
    cld     $t1, $zero, HEADER_END_OFFSET(queue)          # $t1 = end
    bne    $t0, $t1, pop                                  # $t0 != $t1

# TODO in a multicore world we may wish to spin a few times before resorting to WAIT
    li      $v0, WAIT                                     #
    syscall                                               # SYSCALL(WAIT)

pop:
    sll     $t1, $t0, (CAP_SIZE_BITS + 4)                 # 16 capabilities worth of stuff in a message

    # pops message #
    clc     $c3, $t1,  (MSGS_START_OFFSET)(queue)
    clc     $c4, $t1,  (MSGS_START_OFFSET + CAP_SIZE)(queue)
    clc     $c5, $t1,  (MSGS_START_OFFSET + 2 * CAP_SIZE)(queue)
    clc     $c6, $t1,  (MSGS_START_OFFSET + 3 * CAP_SIZE)(queue)
    clc     $c7, $t1,  (MSGS_START_OFFSET + 4 * CAP_SIZE)(queue)
    clc     $c8, $t1,  (MSGS_START_OFFSET + 5 * CAP_SIZE)(queue)
    clc     $c9, $t1,  (MSGS_START_OFFSET + 6 * CAP_SIZE)(queue)
    clc     $c10, $t1, (MSGS_START_OFFSET + 7 * CAP_SIZE)(queue)
    clc     $idc, $t1, (MSGS_START_OFFSET + 8 * CAP_SIZE)(queue)
    clc     $c1, $t1,  (MSGS_START_OFFSET + 9 * CAP_SIZE)(queue)
    clc     $c2, $t1,  (MSGS_START_OFFSET + 10 * CAP_SIZE)(queue)
    cld     $a0, $t1,  (MSGS_START_OFFSET + CAP_IN_MESSAGE * CAP_SIZE)(queue)
    cld     $a1, $t1,  (MSGS_START_OFFSET + CAP_IN_MESSAGE * CAP_SIZE + 8)(queue)
    cld     $a2, $t1,  (MSGS_START_OFFSET + CAP_IN_MESSAGE * CAP_SIZE + 16)(queue)
    cld     $a3, $t1,  (MSGS_START_OFFSET + CAP_IN_MESSAGE * CAP_SIZE + 24)(queue)
    cld     $v0, $t1,  (MSGS_START_OFFSET + CAP_IN_MESSAGE * CAP_SIZE + 32)(queue)

    cld     $t1, $zero, HEADER_LEN_OFFSET(queue)        #
    addi    $t0, $t0, 1                                 #
    bne    $t0, $t1, update_start                       #
    nop                                                 #
    li      $t0, 0                                     # $t0 = ($t0 + 1) % len

update_start:
    csd     $t0, $zero, HEADER_START_OFFSET(queue)      # start = $t0

	bltz		$v0, control
	nop

	dla		$t0, msg_methods
	dla		$t1, msg_methods_nb
	b		call
	nop

control:
	dla		$t0, ctrl_methods
	dla		$t1, ctrl_methods_nb
	sub		$v0, $0, $v0
call:
	cld		$t1, $t1, 0(def_cap)
	sub		$t1, $t1, $v0
	blez		$t1, overflow
	nop

	#Save sync_token
	dla		$t1, sync_token
	csc		$c1, $t1, 0(def_cap)
    #Save caller
    dla     $t1, sync_caller
    csc     $c2, $t1, 0(def_cap)

	dsll	$v0, $v0, CAP_SIZE_BITS
	dadd	$t0, $t0, $v0
	clc		$c12, $t0, 0(def_cap)
	cjalr		$c12, $c17
	nop

	#Restore sync_token
	dla		$t1, sync_token
	clc		$c1, $t1, 0(def_cap)
	#Restore caller activation reference
	dla     $t1, sync_caller
	clc     $c2, $t1, 0(def_cap)

	b		end
	nop

overflow:
	move		$v0, $0
	move		$v1, $0
	cfromptr	$c3, $c3, $0
	b		end
	nop

end:

#FIXME this instruction cannot jump backwards due to a QEMU bug
	cbts    $c1, make_return
	nop
    b msg_entry_loop
	nop

make_return:
    # c3 is both a return register and an arugment register so needs no work.
    # v0 and v1 need to be arguments.
    # c4 has the token from above.
    # c1 needs the return cap, we kept it in a saved register
    # c2 needs the message reply cap, this is already installed.
    # c17 needs a good return address. We took the address of the loop symbol which is as good as any
    move  $a0, $v0
    move  $a1, $v1
    cmove $c4, $c1
    cmove $c1, message_reply
    cmove $c17, msg_entry_loop_cap
	ccall $c1, $c2
	b msg_entry_loop
    nop

msg_entry_end:
	.end msg_entry
	.size msg_entry, msg_entry_end - msg_entry
