/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lawrence Esswood
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

.macro TRACE_ON
    slti zero, zero, 0x1b
.endm

.macro TRACE_OFF
    slti zero, zero, 0x1e
.endm

.macro START_FUNC name
    .global \name
    ASM_VISIBILITY \name
    .type \name, "function"
    \name:
.endm

.macro END_FUNC name
    \name\()_end:
    .size \name, \name\()_end - \name
.endm

.macro CALL_FUNC sym
    lui         ra, %captab_call_hi(\sym)
    cincoffset  cra, cgp, ra
    clc         cra, %captab_call_lo(\sym)(cra)
    cjalr       cra, cra
.endm

# la is banned because purecap and/or relative.
# But we are using la in conjunction with capability adds in init, so limited use is fair

.macro la_relative reg sym
    lui \reg, %hi(\sym)
    addi \reg, \reg, %lo(\sym)
.endm
