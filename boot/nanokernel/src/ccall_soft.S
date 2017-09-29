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

.set MIPS_SZCAP, _MIPS_SZCAP
.include "asm.S"

.section .trampoline_ccall, "ax"

#define __ASSEMBLY__ 1
#include "exception_cause.h"

#
# Relocatable ccall vector; checks arguments, sets c12 <- pcc <- c1 and idc <- c2
# This runs with KCC installed in PCC.
#

# Creturn does IF(unsealed($c17)) pcc <- $c17 ELSE c18 <- unseal(c18) ; pcc <- unseal(c17)
		.global kernel_ccall_trampoline
		.ent kernel_ccall_trampoline
kernel_ccall_trampoline:
# FIXME remove this when QEMU is fixed
        dla $k0, 1f
        csetoffset  $kr2c, $kcc, $k0
        cjr $kr2c
        nop

        1:
#demux ccall/return

        cgetcause $k0
        srl  $k0, $k0, 8
        andi $k0, 0x1f
        li   $k1, Return_Trap
        beq  $k0, $k1, creturn_soft
        nop

ccall_soft:
        cchecktype $c1, $c2
        cgettype   $k0, $c1
        csetoffset $kr2c, $kdc, $k0

        # unseal c1 and c2
        cunseal $epcc, $c1, $kr2c
        cunseal $idc, $c2, $kr2c

        b end
        nop

creturn_soft:
		cgetsealed  $k0, $c17
		beq         $k0, 0, trusting
        nop

untrusting:
		cgettype   $k0, $c17
        csetoffset $kr2c, $kdc, $k0

        cunseal $epcc, $c17, $kr2c
        cunseal $idc, $c18, $kr2c
        b end
        nop


trusting:
		cmove   $epcc, $17
        cmove   $idc,  $18

end:
        # Counter eret bug
        cgetoffset $k0, $epcc
        dmtc0      $k0, $14

        # return
        eret

kernel_ccall_trampoline_end:
		nop
		.global kernel_ccall_trampoline_end
		.end kernel_ccall_trampoline
		.size kernel_ccall_trampoline, kernel_ccall_trampoline_end - kernel_ccall_trampoline