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

    .data

.align  8
enter_once_lock: .space 8

	.text
	.global start
	.ent start
start:
// This is the secure version of init. c3 will contain the entry_t. c0 will contain c3. idc will contain c0

// First undo this mixup. c0 and c3 will be sensible. idc will contain our entry_t. start_common may want to keep this.

    cmove       $c4, $idc
    cmove       $idc, $c3
    cgetdefault $c3
    csetdefault $c4

// Now enforce start once semantics. The entrance lock is set to 1 and never unset.
// If found set, we just die.

    dla         $t0, enter_once_lock
    cincoffset  $c5, $c4, $t0

    li          $t1, 1

    clld        $t0, $c5
retry:
    bnez        $t0, secure_double_entry
    cscd        $t0, $t1, $c5
    beqz        $t0, retry
    clld        $t0, $c5

// TODO we don't trust our stack, so make a new one and copy all stack arguments

//Finally go to normal start_common
	j start_common
    nop

secure_double_entry:
// FIXME: A better faulting sequence
    cfromptr    $c3, $c3, $zero
    cld         $t0, $zero, 0($c3) // Will trigger an exception