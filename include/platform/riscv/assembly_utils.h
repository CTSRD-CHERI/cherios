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

#ifndef CHERIOS_ASSEMBLY_UTILS_H
#define CHERIOS_ASSEMBLY_UTILS_H

#include "macroutils.h"

#define Reg_Encode_ddc         1
#define Reg_Encode_ra          1
#define Reg_Encode_sp          2
#define Reg_Encode_gp          3
#define Reg_Encode_usp         4

#define Reg_Encode_t0          5
#define Reg_Encode_t1          6
#define Reg_Encode_t2          7

#define Reg_Encode_s0          8
#define Reg_Encode_s1          9

#define Reg_Encode_a0          10
#define Reg_Encode_a1          11
#define Reg_Encode_a2          12
#define Reg_Encode_a3          13
#define Reg_Encode_a4          14
#define Reg_Encode_a5          15
#define Reg_Encode_a6          16
#define Reg_Encode_a7          17

#define Reg_Encode_s2          18
#define Reg_Encode_s3          19
#define Reg_Encode_s4          20
#define Reg_Encode_s5          21
#define Reg_Encode_s6          22
#define Reg_Encode_s7          23
#define Reg_Encode_s8          24
#define Reg_Encode_s9          25
#define Reg_Encode_s10         26
#define Reg_Encode_s11         27
#define Reg_Encode_t3          28
#define Reg_Encode_t4          29
#define Reg_Encode_t5          30
#define Reg_Encode_idc         31

#define Reg_Shift(x)           (1UL << x)
#define Reg_Encode(Reg)         Reg_Shift(Reg_Encode ## _ ## Reg)

#define Expand_Encode_Help() Reg_Encode_All_Help
#define Expand_Encode_All(X, ...) Reg_Encode(X) | DEFER2(Expand_Encode_Help)()(__VA_ARGS__)
#define Reg_Encode_All_Help(...) IF_ELSE(HAS_ARGS(__VA_ARGS__))(Expand_Encode_All(__VA_ARGS__))(0)
#define Reg_Encode_All(...) EVAL32(Reg_Encode_All_Help(__VA_ARGS__))

#endif //CHERIOS_ASSEMBLY_UTILS_H
