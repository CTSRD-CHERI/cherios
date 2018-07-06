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

// some co-processor regs
#define cp0_index     $0
#define cp0_entryhi   $10
#define cp0_entrylo0  $2
#define cp0_entrylo1  $3
#define cp0_pagemask  $5
#define cp0_random    $1
#define cp0_wired     $6
#define cp0_context   $4
#define cp0_badvaddr  $8

// register numbers

#define Reg_Encode_zero        0

#define Reg_Encode_at          1
#define Reg_Encode_v0          2
#define Reg_Encode_v1          3

#define Reg_Encode_a0          4
#define Reg_Encode_a1          5
#define Reg_Encode_a2          6
#define Reg_Encode_a3          7
#define Reg_Encode_a4          8
#define Reg_Encode_a5          9
#define Reg_Encode_a6          10
#define Reg_Encode_a7          11

#define Reg_Encode_t0          12
#define Reg_Encode_t1          13
#define Reg_Encode_t2          14
#define Reg_Encode_t3          15

#define Reg_Encode_s0          16
#define Reg_Encode_s1          17
#define Reg_Encode_s2          18
#define Reg_Encode_s3          19
#define Reg_Encode_s4          20
#define Reg_Encode_s5          21
#define Reg_Encode_s6          22
#define Reg_Encode_s7          23

#define Reg_Encode_t8          24
#define Reg_Encode_t9          25
#define Reg_Encode_k0          26
#define Reg_Encode_k1          27
#define Reg_Encode_gp          28
#define Reg_Encode_sp          29
#define Reg_Encode_fp          30
#define Reg_Encode_ra          31

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
#define EN9(X,...) EncodeReg(X) | EN8(__VA_ARGS__)
#define EN10(X,...) EncodeReg(X) | EN9(__VA_ARGS__)
#define EN11(X,...) EncodeReg(X) | EN10(__VA_ARGS__)
#define EN12(X,...) EncodeReg(X) | EN11(__VA_ARGS__)
#define EN13(X,...) EncodeReg(X) | EN12(__VA_ARGS__)


#define cop0_encode (0b010000 << 26)
#define mftr_encode (0b01000 << 21)
#define mttr_encode (0b01100 << 21)

#define MFTC0_I(rd, rt, sel) .word (cop0_encode | mftr_encode | (rt << 16) | (EncodeReg(rd) << 11) | sel)

#define MFTC0(...) MFTC0_I(__VA_ARGS__)


#define MTTC0_I(rt, rd, sel) .word (cop0_encode | mttr_encode | (EncodeReg(rt) << 16) | (rd << 11) | sel)
#define MTTC0(...) MTTC0_I(__VA_ARGS__)

#define DVPE(rt) \
.word (0b01000001011 << 21)  | (EncodeReg(rt) << 16) |  0b0000000000000001; \
ehb

#define EVPE .word   0x41600021; \
ehb


#define SEND_IPI(tmp, n) \
    MFTC0(tmp, 13, 0); \
    ori $ ## tmp, $ ## tmp, (1 << (MIPS_CP0_STATUS_IM_SHIFT + n)); \
    MTTC0(tmp, 13, 0); \

#define FORK(rs,rt,rd) .word (0b011111 << 26) | (EncodeReg(rs) << 21) | (EncodeReg(rt) << 16) | (EncodeReg(rd) << 11) | (0b001000)



#endif //CHERIOS_ASSEMBLY_UTILS_H
