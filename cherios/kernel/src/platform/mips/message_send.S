# -
# Copyright (c) 2018 Lawrence Esswood
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
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
.set MIPS_SZCAP, _MIPS_SZCAP
#include "mips.h"
#include "assembly_utils.h"
#include "asm.S"
#include "dylink.h"
#include "syscall_ret.h"
#include "kernel.h"
#include "activations.h"
#include "sched.h"
#include "syscalls.h"

# Message send is still a bit funky because of how it returns, should probably fix this
# This does the normal cross domain stuff, but creates a struct to contain both possible return registers and
# loads them before returning to the caller

.text

#if (KERNEL_FASTPATH)

.weak critical_section_enter_dummy
.weak critical_section_exit_dummy
.weak context_switch_dummy

.global __cross_domain_kernel_message_send
//.ent __cross_domain_kernel_message_send
__cross_domain_kernel_message_send:

# highly optimised fastpath. Tries to just call context switch with arguments.
# NEED: Target in fastpath receive. Target on same CPU. Send and Switch or Sync Send.
li                  $t9, ACT_BIG_BIAS

daddiu              $t0, $a4, -SYNC_CALL
bnez                $t0, slow_path

clcbi               $c25, (CTLP_OFFSET_CGP)($idc)

# Unseal target
get_sym             $c1, ref_sealer
li                  $t3, MAX_SEQ_NS
clcbi               $c1, 0($c1)
cld                 $t0, $t9, (ACT_SYNC_TOKEN_OFFSET-ACT_BIG_BIAS)($idc)     # seq n
cunseal             $c15, $c7, $c1

# Check now that it is possible construct a sequence token without alloc, put it in c8 for now.
clc                 $c13, $zero, ACT_SYNC_IND_OFFSET($idc)      # indirection
cld                 $t1, $zero, CAP_SIZE($c13)                  # ind add
get_sym             $c14, sync_token_sealer
dsubu               $t2, $t0, $t1                               # off amount
beq                 $t2, $t3, slow_path
daddiu              $t2, $t2, MIN_OFFSET
clcbi               $c14, 0($c14)
csetoffset          $c13, $c13, $t2
cseal               $c8, $c13, $c14

no_token:

get_sym             $c2, nano_kernel_if_t_data_obj
get_sym             $c1, critical_section_enter_dummy
cmove               $c13, $c17
cmove               $c14, $c18

# Enter critical. v0 will be cpu. enter causes no clobers
ccall_link

# Check we are on the same pool as target

clb                 $t0, $t9, (ACT_POOL_ID_OFFSET-ACT_BIG_BIAS)($c15)
bne                 $t0, $v0, bailout

li                  $t0, ACT_SCHED_LOCK_OFFSET
cincoffset          $c1, $c15, $t0


# take out targets sched lock while checking fastpath is supported
1:
cllb                $t0, $c1
bnez                $t0, 1b
clw                 $t1, $t9, (ACT_SCHED_SCHED_STATUS_OFFSET-ACT_BIG_BIAS)($c15)
andi                $t1, $t1, 0x400 # TODO sched_wait_fastpath
beqz                $t1, bailout
li                  $t0, 1
cscb                $t0, $t0, $c1
beqz                $t0, 1b
# ok to keep slot clear

# we are now locked into the fast path. First do things with the scheduler.

li                  $t0, 1
csw                 $t0, $t9, (ACT_CONTEXT_SYNC_COND_OFFSET-ACT_BIG_BIAS)($idc)

move                $v1, $ra

# Swap which activation is marked as running
bal                 sched_q_swap_subroutine
li                  $t1, 0x20  # TODO sched_sync_nlock

move                $ra, $v1

# Now release lock on target
csb                 $zero, $t9, (ACT_SCHED_LOCK_OFFSET-ACT_BIG_BIAS)($c15)

# use context switch, will take us straight back to c17/c18

get_sym             $c1, context_switch_dummy


# fiddle argument order

cmove               $c7, $c8            # c7 will becomes c1, already constructed above into c8
clcbi               $c8, ACT_CONTEXT_OFFSET($c15)
move                $a4, $a5            # becomes v0
li                  $a5, FAST_RES_FAST  # becomes v1

cmove               $c17, $c13
cmove               $c18, $c14

cclearhi            EN1(c25)
cclearlo            EN4(c13, c14, c15, c16)

ccall_slotless      $c1, $c2, 2

bailout:

# need to exit critical section
get_sym             $c1, critical_section_exit_dummy
ccall_link
cmove               $c17, $c13
cmove               $c18, $c14

# then fall through to slow path

slow_path:
clcbi  $c14, (CTLP_OFFSET_CDL)($idc)
cjr     $c14
cgetpcc $c12
clcbi  $c11, (CTLP_OFFSET_CSP)($idc)
clcbi  $c10, (CTLP_OFFSET_CUSP)($idc)
cincoffset  $c11, $c11, -((2 * CAP_SIZE))
csc	$c17, $zero, (0)($c11)
csc	$c18, $zero, (CAP_SIZE)($c11)
clcbi  $c12, %capcall20(kernel_message_send_ret)($c25)
cjalr  $c12, $c17
cmove  $c18, $idc
clc	$c17, $zero, (0)($c11)
clc	$c18, $zero, (CAP_SIZE)($c11)
cld $v0, $zero, (RET_T_v0_offset)($c3)
cld $v1, $zero, (RET_T_v1_offset)($c3)
clc	$c3, $zero, 0($c3)
cincoffset $c11, $c11, ((2 * CAP_SIZE))
ccall_slotless  $c17, $c18, 2

//.end __cross_domain_kernel_message_send


// fastpath also needs writing in assembly because it is allowed to return a message in argument registers

#(capability c3, register_t v0 (a0), register_t v1 (a1), act_reply_kt reply_token (c4), int64_t timeout (a2), int notify_is_timeout(a3)
.global __cross_domain_kernel_fastpath_wait
//.ent __cross_domain_kernel_fastpath_wait
__cross_domain_kernel_fastpath_wait:

li                  $t9, ACT_BIG_BIAS

# If there is no hint, or we can't use it, go to a slow path

clcbi               $c25, (CTLP_OFFSET_CGP)($idc)

cbez                $c4, fastwait_bail  # We require something to switch to for the fast path
get_sym             $c12, __cross_domain_fastpath_bailout
bnez                $a2, fastwait_bail # currently if a timeout or notify as timeout is provided, we also bail
get_sym             $c13, sync_token_sealer

get_sym             $c2, nano_kernel_if_t_data_obj

# unseal token
clcbi               $c13, 0($c13)
cunseal             $c15, $c4, $c13
cgetoffset          $t0, $c15
daddiu              $t0, $t0, -MIN_OFFSET
csetoffset          $c15, $c15, $zero
cld                 $t1, $zero, CAP_SIZE($c15)
clc                 $c15, $zero, 0($c15)          # caller <- switch to this
daddu               $t0, $t0, $t1               # sequence number

# Enter critical. v0 will be cpu. enter causes no clobers
get_sym             $c1, critical_section_enter_dummy
cmove               $c13, $c17
cmove               $c14, $c18
ccall_link

# check on same CPU
clb                 $t1, $t9, (ACT_POOL_ID_OFFSET-ACT_BIG_BIAS)($c15)
bne                 $t1, $v0, fastwait_bailwithexit

# CAS to check token
li                  $t1, ACT_SYNC_TOKEN_OFFSET
cincoffset          $c1, $c15, $t1

1: clld             $t1, $c1
bne                 $t0, $t1, fastwait_bailwithexit     # call bail for error handling. It will recognise the same error.
daddiu              $t1, $t1, 1
cscd                $t1, $t1, $c1
beqz                $t1, 1b
li                  $t1, (0x10 | 0x400) # hoisted here, need for sched_q_swap_subroutine TODO sched_waiting | sched_wait_fastpath

daddiu              $t0, $t1, 0x80 # TODO sched_notify
movn                $t1, $t0, $a3   # If notify_is_timeout add in sched_wait_notify
# now locked in to fastpath


# We may well not be returning that did not do a fastpath send. In which case need to also store return

csc                 $c3, $t9, (ACT_C3_OFFSET-ACT_BIG_BIAS)($c15)
csd                 $a0, $t9, (ACT_V0_OFFSET-ACT_BIG_BIAS)($c15)
csd                 $a1, $t9, (ACT_V1_OFFSET-ACT_BIG_BIAS)($c15)



move                $v1, $ra
# set self to waiting on message | fastpath message, and do scheduler queue swap
bal                 sched_q_swap_subroutine
# Set target sync condition to 0
csw                 $zero, $t9, (ACT_CONTEXT_SYNC_COND_OFFSET-ACT_BIG_BIAS)($c15)

move                $ra, $v1

fast_wait_end:

cmove               $c17, $c13
cmove               $c18, $c14

# clear any regs we used, then call context switch. Will return directly to user with arguments provided.

get_sym             $c1, context_switch_dummy
clcbi               $c8, ACT_CONTEXT_OFFSET($c15)

cclearhi            EN1(c25)
cclearlo            EN8(c4, c5, c6, c7, c12, c13, c14, c15)

# This is very confusing. Fastpath wait has what should be v0/v1 in a0/a1. Context swtich expects arguments in a weird order
move                $a4, $a0 # a4 becomes v0, a0 has v0
move                $a5, $a1 # a5 becomes v1, a1 has v1
ccall_slotless      $c1, $c2, 2

fastwait_bailwithexit:
get_sym             $c1, critical_section_exit_dummy
ccall_link
cmove               $c17, $c13
cmove               $c18, $c14
fastwait_bail:
cjr                 $c12
nop



# This will remove the IDC activation from the v0 queue, adds c15 activation to queue
# FIXME IDC needs locking?. c15 should already be locked.
# c15 will be set running
# idc will be set to $t1

# clobbers a bunch of temps, c1, c9, c17, c18
sched_q_swap_subroutine:

# get sched pool
get_sym             $c1, sched_pools
li                  $t0, SCHED_POOL_SIZE
mult                $v0, $t0
mflo                $t0
cincoffset          $c1, $c1, $t0               # c1 is pool

# take out lock queue
cincoffset          $c17, $c1, SCHED_POOL_LOCK_OFFSET
1:
cllb                $t3, $c17
bnez                $t3, 1b
li                  $t3, 1
cscb                $t3, $t3, $c17
beqz                $t3, 1b
nop

# First delete the idc act

# LEVEL_TO_NDX(level) ((level > PRIO_IO) ? PRIO_IO : level)
clw                 $t2, $t9, (ACT_PRIO_OFFSET-ACT_BIG_BIAS)($idc)
li                  $t3, 4 # TODO PRIO_IO
sltu                $t8, $t3, $t2
movn                $t2, $t3, $t8
dsll                $t2 , $t2 , SCHED_QUEUE_SIZE_BITS
cincoffset          $c9, $c1, $t2           # c9 + SCHED_POOL_QUEUES_OFFSET is queue

# get queue offsets, also decrement end
clb                 $t3, $zero, (SCHED_QUEUE_END_OFFSET+SCHED_POOL_QUEUES_OFFSET)($c9)
clb                 $t0, $t9, (ACT_Q_NDX_OFFSET-ACT_BIG_BIAS)($idc)
daddiu              $t3, $t3, -1
csb                 $t3, $zero, (SCHED_QUEUE_END_OFFSET+SCHED_POOL_QUEUES_OFFSET)($c9)
dsll                $t3, $t3, CAP_SIZE_BITS
dsll                $t8, $t0, CAP_SIZE_BITS

# set deleted index to end, set end to NULL
clc                 $c18, $t3, (SCHED_POOL_QUEUES_OFFSET + SCHED_QUEUE_ARRAY_OFFSET)($c9) # sched_q[end]
csc                 $cnull, $t3, (SCHED_POOL_QUEUES_OFFSET + SCHED_QUEUE_ARRAY_OFFSET)($c9)
csc                 $c18, $t8, (SCHED_POOL_QUEUES_OFFSET + SCHED_QUEUE_ARRAY_OFFSET)($c9) # sched_q[del_index] = sched_q[end]
csb                 $t0, $t9, (ACT_Q_NDX_OFFSET-ACT_BIG_BIAS)($c18)

# Now add $c15 in

clw                 $t2, $t9, (ACT_PRIO_OFFSET-ACT_BIG_BIAS)($c15)
li                  $t3,  4 # TODO PRIO_IO
sltu                $t8, $t3, $t2
movn                $t2, $t3, $t8
dsll                $t2 , $t2 , SCHED_QUEUE_SIZE_BITS
cincoffset          $c9, $c1, $t2           # c9 + SCHED_POOL_QUEUES_OFFSET is queue

clb                 $t3, $zero, (SCHED_QUEUE_END_OFFSET+SCHED_POOL_QUEUES_OFFSET)($c9)
csb                 $t3, $t9, (ACT_Q_NDX_OFFSET-ACT_BIG_BIAS)($c15)
dsll                $t2, $t3, CAP_SIZE_BITS
daddiu              $t3, $t3, 1
csc                 $c15, $t2, (SCHED_POOL_QUEUES_OFFSET + SCHED_QUEUE_ARRAY_OFFSET)($c9)
csb                 $t3, $zero, (SCHED_QUEUE_END_OFFSET+SCHED_POOL_QUEUES_OFFSET)($c9)

# Also set pools current act

csc                 $c15, $zero, SCHED_POOL_CURRENT_ACT_OFFSET($c1)

# Source is now X
csw                 $t1, $t9, (ACT_SCHED_SCHED_STATUS_OFFSET-ACT_BIG_BIAS)($idc)

# Target is now running
li                  $t1, 1 # TODO sched_running
csw                 $t1, $t9, (ACT_SCHED_SCHED_STATUS_OFFSET-ACT_BIG_BIAS)($c15)


# jump back and release lock on queue
sync
jr                  $ra
csb                 $zero, $zero, 0($c17)


# might need these

// .size  __cross_domain_kernel_message_send, 0x100
// .size __cross_domain_kernel_fastpath_wait, 0x100

//.end __cross_domain_kernel_fastpath_wait

#else // not Fastpath enabled

.global __cross_domain_kernel_message_send
.ent __cross_domain_kernel_message_send
__cross_domain_kernel_message_send:
clcbi  $c14, (CTLP_OFFSET_CDL)($idc)
cjr     $c14
cgetpcc $c12
clcbi  $c11, (CTLP_OFFSET_CSP)($idc)
clcbi  $c10, (CTLP_OFFSET_CUSP)($idc)
clcbi  $c25, (CTLP_OFFSET_CGP)($idc)
cincoffset  $c11, $c11, -((2 * CAP_SIZE))
csc	$c17, $zero, (0)($c11)
csc	$c18, $zero, (CAP_SIZE)($c11)
clcbi  $c12, %capcall20(kernel_message_send_ret)($c25)
cjalr  $c12, $c17
cmove  $c18, $idc
clc	$c17, $zero, (0)($c11)
clc	$c18, $zero, (CAP_SIZE)($c11)
cld $v0, $zero, (RET_T_v0_offset)($c3)
cld $v1, $zero, (RET_T_v1_offset)($c3)
clc	$c3, $zero, 0($c3)
cincoffset $c11, $c11, ((2 * CAP_SIZE))
ccall_slotless  $c17, $c18, 2
.end __cross_domain_kernel_message_send



.global __cross_domain_kernel_fastpath_wait
.ent __cross_domain_kernel_fastpath_wait
__cross_domain_kernel_fastpath_wait:

clcbi               $c25, (CTLP_OFFSET_CGP)($idc)
get_sym             $c12, __cross_domain_fastpath_bailout
cjr                 $c12
nop

.end __cross_domain_kernel_fastpath_wait

#endif