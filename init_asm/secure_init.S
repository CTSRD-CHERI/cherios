#-
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
#include "syscalls.h"
#include "cprogram.h"
#include "elf.h"

	.text
	.global start
	.ent start
start:

# This is the secure version of init.

# See init.S for most of the arguments. Some we will have to construct ourself.

# The following arguments are no longer provided (or trusted) and this assembly calculates them:

# c11/c10 (stack)
# c4 (seg table)

# tls_seg_offset (s1)
# tls seg size (s5)
# c5 (tls proto)
# size of tls proto (s3)

# code_segment_offset (a4)
# c6 (code write cap)

# data_seg_offset (a2)

# And the new arguments:

# idc/c9 : probably null
# c8    : our contiguous data (provided by foundation_enter)
# c2    : our auth key

# First build a segment table + segment addr table
# WARN: We assume the order of segments is code/data and there are no there are no others (aprt from the one TLS)

    #
    cmove       $c9, $c2                            # auth key going in c9
    # Create a stack for the first thread (just stored in our data section)
    la_relative  $t0, secure_stack
    li          $t1, DEFAULT_STACK_SIZE
    cincoffset  $c11, $c8, $t0
    cincoffset  $c13, $c11, $t1                     # the lock. Easier to materialise here
    csetboundsexact $c11, $c11, $t1
    cincoffset  $c11, $c11, $t1
    cnull       $c10


    # Take the lock or fail
    li          $t0, 1
1:  cllb        $t1, $c13
    bnez        $t1, secure_double_entry
    cscb        $t1, $t0, $c13
    beqz        $t1, 1b
    li          $a7, 0                              # a7 will be where our first (non proto) tls segment is

    # Now construct a segment tabble. We loop through our program headers to fill it out.
    # This table should go on the stack. CRT will copy it, and then reclaim the space.

    cincoffset  $c11, $c11, -(MAX_SEGS * (CAP_SIZE + REG_SIZE))
    csetbounds  $c4, $c11, MAX_SEGS * (CAP_SIZE + REG_SIZE)

    clh         $t0, $zero, EHDR_OFF_e_phnum($c8)   # t0 = e_phnum
    cld         $t1, $zero, EHDR_OFF_e_phoff($c8)   # t1 = e_phoff

    cincoffset  $c13, $c8, $t1                      # c13 = phdrs_start

    li          $t1, 0                              # t1 = index (we add one before storing in seg table)
    dsll        $t0, $t0, CAP_SIZE_BITS             # we loop over offsets into the seg table

    # This loops over program headers

loop_start:
    clw         $t2, $zero, PHDR_OFF_p_type($c13)   # P_TYPE: PT_LOAD goes in table. PT_TLS gives us info (proto tls, tls_seg_vaddr. tls proto size)
    daddiu      $t2, $t2, (-PT_LOAD)                # check if its a PT_LOAD
    daddiu      $t1, $t1, CAP_SIZE                   # t1 incremented here for all paths
    cld         $t8, $zero, PHDR_OFF_p_vaddr($c13)  # t8 = vaddr
    cld         $t9, $zero, PHDR_OFF_p_memsz($c13)  # t9 = memsz
    bnez        $t2, is_not_pt_load
    cincoffset  $c14, $c8, $t8                      # c14 points at the vaddr

is_pt_load:

    clw         $t2, $zero, PHDR_OFF_p_flags($c13)  # t2 = flags (we assume W is data otherwise code)
    csetbounds  $c14, $c14, $t9                     # c14 is out segment

    daddu       $t9, $t9, $t8                       # end of this segment
    sltu        $at, $a7, $t9                       # at = 1 if t9 larger
    movn        $a7, $t9, $at                       # a7 = max(a7, t9)

    andi        $t2, $t2, PF_W
    dsrl        $at, $t1, (CAP_SIZE_BITS - REG_SIZE_BITS)
    csd         $t8, $at, (MAX_SEGS * CAP_SIZE)($c4)    # store vaddr in table
    bnez        $t2, is_w
    csc         $c14, $t1, 0($c4)                       # store seg cap in table
is_e:
    b           loop_footer
    move        $a4, $t1                            # set code_seg_offset
is_w:
    b           loop_footer
    move        $a2, $t1                            # set data_seg_offset


is_not_pt_load:
    daddiu      $t2, $t2, (PT_LOAD-PT_TLS)          # check if its a PT_TLS
    bnez        $t2, loop_footer
    nop

is_tls:
    # We want the executable version in the seg table for the code segment. Swap it.
    cld         $s3, $zero, PHDR_OFF_p_filesz($c13) # tls proto size
    move        $s1, $t1                            # tls seg offset
    dsrl        $at, $t1, (CAP_SIZE_BITS - REG_SIZE_BITS)
    csd         $t8, $at, (MAX_SEGS * CAP_SIZE)($c4)    # store tls vaddr in table
    move        $s5, $t9                            # _real_ tls memsize. We use this later
    csetbounds  $c5, $c14, $s3                      # tls proto

loop_footer: # with checks inlined at all the ends of the loop
    bne         $t0, $t1, loop_start
    cincoffset  $c13, $c13, PHDR_SIZE


    # Build _real_ tls segment and store in table
    # a7 is where the last of our segments ended. Round up to ensure alignment.
    daddiu      $a7, $a7, (CAP_SIZE-1)
    dli         $t0, ~(CAP_SIZE-1)
    and         $a7, $a7, $t0
    cincoffset  $c13, $c8, $a7
    csetbounds  $c13, $c13, $s5
    csc         $c13, $s1, 0($c4)

    # Create an exe version of code segment cap and swap it with table

    clc         $c6, $a4, 0($c4)
    cgetpcc     $c13
    cgetaddr    $t0, $c6
    cgetaddr    $t1, $c13
    dsubu       $t0, $t0, $t1
    cincoffset  $c13, $c13, $t0
    cgetlen     $t0, $c6
    csetbounds  $c13, $c13, $t0
    csc         $c13, $a4, 0($c4)

    # TODO (maybe). Do we want to create our on own queue? Currently it was given to us. We can probably do this later

# Now we can go to common code
    jmp_in_segment start_common, $at, $c1
    cmove $c8, $c16
    cmove       $c8, $c16                                 # c16 had what should be in c8 on entry to common

.end start

secure_double_entry:
    trap

.bss
    .p2align DEFAULT_STACK_ALIGN_p2
    .global secure_stack
secure_stack:
    .space DEFAULT_STACK_SIZE
    .size secure_stack, DEFAULT_STACK_SIZE
entry_lock:
    .space 1