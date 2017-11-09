/*-
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
.include "asm.S"

.text
.global cheri_invoke

# See demo.S for how the ABI will eventually look. This is still a placeholder.


cheri_invoke:
    # c17/c18 will already be setup with the correct return address / data.

    # This implements the new ABI for the caller

    # save c17 / c18
    cincoffset  $c11, $c11, -(2 * CAP_SIZE)
    csc         $c17, $zero, 0($c11)
    csc         $c18, $zero, (CAP_SIZE)($c11)

    cmove   $idc, $c11          # idc and c11 should have been the same
    cmove   $c18, $idc          # set return data
    cgetpccincoffset $c17, 12    # set return code
    ccall   $c1, $c2, 2
    nop

invoke_return:
    cmove   $c11, $idc          # idc and c11 should have been the same
    clc     $c17, $zero, 0($c11)
    clc     $c18, $zero, (CAP_SIZE)($c11)
    cjr     $c17
    cincoffset $c11, $c11, (2 * CAP_SIZE)