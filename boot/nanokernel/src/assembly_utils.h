/*-
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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

#ifndef CHERIOS_ASSEMBLY_UTILS_H
#define CHERIOS_ASSEMBLY_UTILS_H

// To make cclearhi and cclearlo easier to read

#define Reg_Encode_all (1 << 16) - 1
#define Reg_Encode_c0 1
#define Reg_Encode_c1 2
#define Reg_Encode_c2 4
#define Reg_Encode_c3 8
#define Reg_Encode_c4 0x10
#define Reg_Encode_c5 0x20
#define Reg_Encode_c6 0x40
#define Reg_Encode_c7 0x80
#define Reg_Encode_c8 0x100
#define Reg_Encode_c9 0x200
#define Reg_Encode_c10 0x400
#define Reg_Encode_c11 0x800
#define Reg_Encode_c12 0x1000
#define Reg_Encode_c13 0x2000
#define Reg_Encode_c14 0x4000
#define Reg_Encode_c15 0x8000
#define Reg_Encode_c16 1
#define Reg_Encode_c17 2
#define Reg_Encode_c18 4
#define Reg_Encode_c19 8
#define Reg_Encode_c20 0x10
#define Reg_Encode_c21 0x20
#define Reg_Encode_c22 0x40
#define Reg_Encode_c23 0x80
#define Reg_Encode_c24 0x100
#define Reg_Encode_c25 0x200
#define Reg_Encode_c26 0x400
#define Reg_Encode_c27 0x800
#define Reg_Encode_c28 0x1000
#define Reg_Encode_c29 0x2000
#define Reg_Encode_c30 0x4000
#define Reg_Encode_c31 0x8000

#define Reg_Encode_kr1c Reg_Encode_c27
#define Reg_Encode_kr2c Reg_Encode_c28
#define Reg_Encode_kcc  Reg_Encode_c29
#define Reg_Encode_kdc  Reg_Encode_c30
#define Reg_Encode_epcc Reg_Encode_c31

#define EncodeReg(Reg) (Reg_Encode ## _ ## Reg)

#define EN1(X) EncodeReg(X)
#define EN2(X,...) EncodeReg(X) | EN1(__VA_ARGS__)
#define EN3(X,...) EncodeReg(X) | EN2(__VA_ARGS__)
#define EN4(X,...) EncodeReg(X) | EN3(__VA_ARGS__)
#define EN5(X,...) EncodeReg(X) | EN4(__VA_ARGS__)
#define EN6(X,...) EncodeReg(X) | EN5(__VA_ARGS__)
#define EN7(X,...) EncodeReg(X) | EN6(__VA_ARGS__)
#define EN8(X,...) EncodeReg(X) | EN7(__VA_ARGS__)

#endif //CHERIOS_ASSEMBLY_UTILS_H
