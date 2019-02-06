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

	.text
	.global start
	.ent start
start:

# This is the secure version of init.

# See init.S for most of the arguments. Some we will have to construct ourself from a few new extra arguments

# The following arguments are no longer provided: c4 (seg table), c5 (tls proto), c6 (code write cap)
# The following are but shouldn't: c11 (stack), ??c20 (queue)??

# And the new arguments:

# idc/c9 : probably null
# c8    : or contiguous data
# a7    : the size of the segments before the tls segment
# s0    : the size of the tls segment

# First build a segment table
# WARN: We assume the order of segments is code/data and there are no there are no others (aprt from the one TLS)

    dla         $t0, crt_segment_table  # This is global so we can do relative to idc
    cincoffset  $c4, $c8, $t0
.set MAX_SEGS, 4 # FIXME. Grab from the correct header
    csetbounds  $c4, $c4, MAX_SEGS * CAP_SIZE

    cincoffset  $c13, $c8, $a5      # increment to code_vaddr
    dsubu       $t0, $a3, $a5       # diff between data and code
    csetbounds  $c6, $c13, $t0      # code_write_cap

    cgetpcc     $c13                # derive the same cap but from pcc
    cgetaddr    $t1, $c6
    cgetaddr    $t2, $c13
    dsubu       $t1, $t1, $t2
    cincoffset  $c13, $c13, $t1
    csetbounds  $c13, $c13, $t0
    csc         $c13, $a4, 0($c4)   # Code segment

    beqz        $s1, no_tls
    cnull       $c5

    cincoffset  $c13, $c8, $a7      # increment to TLS section we allocated for the first thread
    csetbounds  $c13, $c13, $s0     # size of tls seg
    csc         $c13, $s1, 0($c4)   # Data segment
    cincoffset  $c5, $c8, $a6       # tls_proto
    csetbounds  $c5, $c5, $s3       # tls_proto is sized by the file size

no_tls:

# Now enforce start once semantics. We use will use setting the data segment as a lock

    cincoffset  $c13, $c8, $a3      # increment to data_vaddr
    dsubu       $t0, $a7, $a3       # diff between size and data
    csetbounds  $c13, $c13, $t0

    cincoffset  $c14, $c4, $a2      # seg_table data entry

retry_set:
    cllc        $c15, $c14
    cbts        $c15, secure_double_entry
    cscc        $t0, $c13, $c14     # Data segment
    beqz        $t0, retry_set
    nop

# TODO we don't trust our stack, so make a new one and copy all stack arguments

# Now we can go to common code
	j start_common
    nop

secure_double_entry:
    trap