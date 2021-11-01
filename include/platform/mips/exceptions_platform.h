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

#ifndef CHERIOS_EXCEPTIONS_PLATFORM_H
#define CHERIOS_EXCEPTIONS_PLATFORM_H

#ifndef __ASSEMBLY__

typedef struct {
    register_t	mf_at, mf_v0, mf_v1;
    register_t	mf_a0, mf_a1, mf_a2, mf_a3, mf_a4, mf_a5, mf_a6, mf_a7;
    register_t	mf_t0, mf_t1, mf_t2, mf_t3;
    register_t	mf_t8, mf_t9;
    register_t	mf_gp, mf_sp, mf_fp, mf_ra;
    register_t	mf_hi, mf_lo;

    register_t  padding;

    capability c2,c3,c4,c5,c6,c7,c8,c9, c12, c13,c14,c15,c16,c17,c18,c25;
#ifdef USE_EXCEPTION_UNSAFE_STACK
    capability c10;
#endif
#ifdef USE_EXCEPTION_STACK
    capability c11;
#endif
} exception_restore_frame;

// Some handler may need access to these
typedef struct {
    register_t mf_s0, mf_s1, mf_s2, mf_s3, mf_s4, mf_s5, mf_s6, mf_s7;
    capability c19, c20, c21, c22, c23, c24;
} exception_restore_saves_frame;

typedef int handler_t(register_t cause, register_t ccause, exception_restore_frame* restore_frame);

typedef int handler2_t(register_t cause, register_t ccause, exception_restore_frame* restore_frame,
                       exception_restore_saves_frame* saves_frame);

// Dont know how to get relocations with offsets in C so...
#define INC_STACK(SN, I)    \
        __asm__ (\
                "clcbi  $c1, %%captab_tls20("SN")($c26)      \n"\
                "li     $t0, %[im]                                      \n"\
                "cincoffset $c1, $c1, $t0                               \n"\
                "cscbi  $c1, %%captab_tls20("SN")($c26)      \n"\
                    :\
                    : [im]"i"(I)\
                    : "t0", "$c1"\
                )

#define N_USER_EXCEPTIONS MIPS_CP0_EXCODE_NUM
#define N_USER_CAP_EXCEPTIONS CAP_CAUSE_NUM

#endif // __ASSEMBLY__

#endif //CHERIOS_EXCEPTIONS_PLATFORM_H
