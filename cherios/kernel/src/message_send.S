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
#include "asm.S"
#include "dylink.h"
#include "syscall_ret.h"

// Message send is still a bit funky because of how it returns, should probably fix this
// This does the normal cross domain stuff, but creates a struct to contain both possible return registers and
// loads them before returning to the caller

.text
.global __cross_domain_kernel_message_send
__cross_domain_kernel_message_send:
clcbi  $c14, (CTLP_OFFSET_CDL)($idc)
cjalr  $c14, $c12
clcbi  $c11, (CTLP_OFFSET_CSP)($idc)
clcbi  $c10, (CTLP_OFFSET_CUSP)($idc)
clcbi  $c25, (CTLP_OFFSET_CGP)($idc)
cincoffset  $c11, $c11, -((2 * CAP_SIZE) + RET_T_size)
cmove       $c8, $c11
csc	$c17, $zero, (RET_T_size)($c11)
csc	$c18, $zero, (RET_T_size + CAP_SIZE)($c11)
clcbi  $c12, %capcall20(kernel_message_send_ret)($c25)
cjalr  $c12, $c17
cmove  $c18, $idc
clc	$c17, $zero, (RET_T_size)($c11)
clc	$c18, $zero, (RET_T_size + CAP_SIZE)($c11)
clc	$c3, $zero, 0($c11)
cld    $v0, $zero, (RET_T_v0_offset)($c11)
cld    $v1, $zero, (RET_T_v1_offset)($c11)
cincoffset $c11, $c11, ((2 * CAP_SIZE) + RET_T_size)
ccall_slotless  $c17, $c18, 2