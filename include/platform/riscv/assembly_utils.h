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

// This is a bit ugly, but allows the preprocessor to get the sizes/indexs of registers

#define Reg_Encode_ddc         0
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

#define Reg_Size_ra             8
#define Reg_Size_sp             8
#define Reg_Size_gp             8
#define Reg_Size_usp            8
#define Reg_Size_t0             8
#define Reg_Size_t1             8
#define Reg_Size_t2             8
#define Reg_Size_s0             8
#define Reg_Size_s1             8
#define Reg_Size_a0             8
#define Reg_Size_a1             8
#define Reg_Size_a2             8
#define Reg_Size_a3             8
#define Reg_Size_a4             8
#define Reg_Size_a5             8
#define Reg_Size_a6             8
#define Reg_Size_a7             8
#define Reg_Size_s2             8
#define Reg_Size_s3             8
#define Reg_Size_s4             8
#define Reg_Size_s5             8
#define Reg_Size_s6             8
#define Reg_Size_s7             8
#define Reg_Size_s8             8
#define Reg_Size_s9             8
#define Reg_Size_s10            8
#define Reg_Size_s11            8
#define Reg_Size_t3             8
#define Reg_Size_t4             8
#define Reg_Size_t5             8

#define Reg_Size_cra            16
#define Reg_Size_csp            16
#define Reg_Size_cgp            16
#define Reg_Size_cusp           16
#define Reg_Size_ct0            16
#define Reg_Size_ct1            16
#define Reg_Size_ct2            16
#define Reg_Size_cs0            16
#define Reg_Size_cs1            16
#define Reg_Size_ca0            16
#define Reg_Size_ca1            16
#define Reg_Size_ca2            16
#define Reg_Size_ca3            16
#define Reg_Size_ca4            16
#define Reg_Size_ca5            16
#define Reg_Size_ca6            16
#define Reg_Size_ca7            16
#define Reg_Size_cs2            16
#define Reg_Size_cs3            16
#define Reg_Size_cs4            16
#define Reg_Size_cs5            16
#define Reg_Size_cs6            16
#define Reg_Size_cs7            16
#define Reg_Size_cs8            16
#define Reg_Size_cs9            16
#define Reg_Size_cs10           16
#define Reg_Size_cs11           16
#define Reg_Size_ct3            16
#define Reg_Size_ct4            16
#define Reg_Size_ct5            16

#define Reg_Size_x0             8
#define Reg_Size_x1             8
#define Reg_Size_x2             8
#define Reg_Size_x3             8
#define Reg_Size_x4             8
#define Reg_Size_x5             8
#define Reg_Size_x6             8
#define Reg_Size_x7             8
#define Reg_Size_x8             8
#define Reg_Size_x9             8
#define Reg_Size_x10            8
#define Reg_Size_x11            8
#define Reg_Size_x12            8
#define Reg_Size_x13            8
#define Reg_Size_x14            8
#define Reg_Size_x15            8
#define Reg_Size_x16            8
#define Reg_Size_x17            8
#define Reg_Size_x18            8
#define Reg_Size_x19            8
#define Reg_Size_x20            8
#define Reg_Size_x21            8
#define Reg_Size_x22            8
#define Reg_Size_x23            8
#define Reg_Size_x24            8
#define Reg_Size_x25            8
#define Reg_Size_x26            8
#define Reg_Size_x27            8
#define Reg_Size_x28            8
#define Reg_Size_x29            8
#define Reg_Size_x30            8
#define Reg_Size_x31            8
#define Reg_Size_x32            8

#define Reg_Size_c0             16
#define Reg_Size_c1             16
#define Reg_Size_c2             16
#define Reg_Size_c3             16
#define Reg_Size_c4             16
#define Reg_Size_c5             16
#define Reg_Size_c6             16
#define Reg_Size_c7             16
#define Reg_Size_c8             16
#define Reg_Size_c9             16
#define Reg_Size_c10            16
#define Reg_Size_c11            16
#define Reg_Size_c12            16
#define Reg_Size_c13            16
#define Reg_Size_c14            16
#define Reg_Size_c15            16
#define Reg_Size_c16            16
#define Reg_Size_c17            16
#define Reg_Size_c18            16
#define Reg_Size_c19            16
#define Reg_Size_c20            16
#define Reg_Size_c21            16
#define Reg_Size_c22            16
#define Reg_Size_c23            16
#define Reg_Size_c24            16
#define Reg_Size_c25            16
#define Reg_Size_c26            16
#define Reg_Size_c27            16
#define Reg_Size_c28            16
#define Reg_Size_c29            16
#define Reg_Size_c30            16
#define Reg_Size_c31            16
#define Reg_Size_c32            16

#define Reg_Cap_To_Int_cra            ra
#define Reg_Cap_To_Int_csp            sp
#define Reg_Cap_To_Int_cgp            gp
#define Reg_Cap_To_Int_cusp           usp
#define Reg_Cap_To_Int_ct0            t0
#define Reg_Cap_To_Int_ct1            t1
#define Reg_Cap_To_Int_ct2            t2
#define Reg_Cap_To_Int_cs0            s0
#define Reg_Cap_To_Int_cs1            s1
#define Reg_Cap_To_Int_ca0            a0
#define Reg_Cap_To_Int_ca1            a1
#define Reg_Cap_To_Int_ca2            a2
#define Reg_Cap_To_Int_ca3            a3
#define Reg_Cap_To_Int_ca4            a4
#define Reg_Cap_To_Int_ca5            a5
#define Reg_Cap_To_Int_ca6            a6
#define Reg_Cap_To_Int_ca7            a7
#define Reg_Cap_To_Int_cs2            s2
#define Reg_Cap_To_Int_cs3            s3
#define Reg_Cap_To_Int_cs4            s4
#define Reg_Cap_To_Int_cs5            s5
#define Reg_Cap_To_Int_cs6            s6
#define Reg_Cap_To_Int_cs7            s7
#define Reg_Cap_To_Int_cs8            s8
#define Reg_Cap_To_Int_cs9            s9
#define Reg_Cap_To_Int_cs10           s10
#define Reg_Cap_To_Int_cs11           s11
#define Reg_Cap_To_Int_ct3            t3
#define Reg_Cap_To_Int_ct4            t4
#define Reg_Cap_To_Int_ct5            t5
#define Reg_Cap_To_Int_c31            x31

#define Reg_Cap_To_Int(X)       Reg_Cap_To_Int_ ## X
#define Reg_Int_To_Cap(X)       c ## X

#define Reg_Shift(x)           (1UL << x)
#define Reg_Encode(Reg)         Reg_Shift(Reg_Encode ## _ ## Reg)
#define Reg_Size(Reg)           Reg_Size_ ## Reg
#define Expand_Encode_Help() Reg_Encode_All_Help
#define Expand_Encode_All(X, ...) Reg_Encode(X) | DEFER2(Expand_Encode_Help)()(__VA_ARGS__)
#define Reg_Encode_All_Help(...) IF_ELSE(HAS_ARGS(__VA_ARGS__))(Expand_Encode_All(__VA_ARGS__))(0)
#define Reg_Encode_All(...) EVAL32(Reg_Encode_All_Help(__VA_ARGS__))

#define AttachSize(R) ,R ,Reg_Size(R)

#define AttachSizes(...) EVAL32(MAP(AttachSize, __VA_ARGS__))

#endif //CHERIOS_ASSEMBLY_UTILS_H
