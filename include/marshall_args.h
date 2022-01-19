/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Lawrence Esswood
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

#ifndef CHERIOS_MARSHAL_ARGS_H
#define CHERIOS_MARSHAL_ARGS_H

#include "macroutils.h"

#define IS_T_CAP(X) (sizeof(X) == sizeof(void*))

#define NTH_CAP_F(res, arg) ( (((SECOND res) == 0) && IS_T_CAP(arg)) ? (void*)(uintptr_t)(arg) : FIRST res, SECOND res - IS_T_CAP(arg))
// Gets the nth cap of a list of arguments, otherwise NULL
#define NTH_CAPH(N, ...) EVAL16(FOLDl(NTH_CAP_F,(NULL,N), __VA_ARGS__))
#define NTH_CAP(...) EVAL1(FIRST NTH_CAPH(__VA_ARGS__))
#define NTH_NCAP_F(res, arg) ( (((SECOND res) == 0) && !IS_T_CAP(arg)) ? (register_t)(uintptr_t)(arg) : FIRST res, SECOND res - !IS_T_CAP(arg))
// Gets the nth non-cap of a list of arguments, otherwise 0
#define NTH_NCAPH(N, ...) EVAL16(FOLDl(NTH_NCAP_F,(0,N), __VA_ARGS__))
#define NTH_NCAP(...) EVAL1(FIRST NTH_NCAPH(__VA_ARGS__))
#define PASS_CAP(X) (void*)(uintptr_t)X

// Gets the nth arg, or else a default
#ifdef MERGED_FILE

// Just pass the arguments padding to 8. The compiler will sort out casts.
    #define ARG0(X, ...) X
    #define ARG1(a,X, ...) X
    #define ARG2(a,b,X, ...) X
    #define ARG3(a,b,c,X, ...) X
    #define ARG4(a,b,c,d,X, ...) X
    #define ARG5(a,b,c,d,e,X, ...) X

    #define ARG6(a,b,c,d,e,f,X, ...) X
    #define ARG7(a,b,c,d,e,f,g, X, ...) X

    #define ARG_ASSERTS(...) _Static_assert(!IS_T_CAP(ARG4(__VA_ARGS__, 0,0,0,0,0,0,0,0)) && \
                                    !IS_T_CAP(ARG5(__VA_ARGS__, 0,0,0,0,0,0,0,0)) &&         \
                                    !IS_T_CAP(ARG6(__VA_ARGS__, 0,0,0,0,0,0,0,0)) &&         \
                                    !IS_T_CAP(ARG7(__VA_ARGS__, 0,0,0,0,0,0,0,0)), "Merged register file passes only the first four arguments as caps");

    #define MARSHALL_ARGUMENTSH(...) (register_t)ARG4(__VA_ARGS__, 0,0,0,0,0,0,0,0), (register_t)ARG5(__VA_ARGS__, 0,0,0,0,0,0,0,0), (register_t)ARG6(__VA_ARGS__, 0,0,0,0,0,0,0,0), (register_t)ARG7(__VA_ARGS__, 0,0,0,0,0,0,0,0), \
                                    PASS_CAP(ARG0(__VA_ARGS__, 0,0,0,0,0,0,0,0)), PASS_CAP(ARG1(__VA_ARGS__, 0,0,0,0,0,0,0,0)), PASS_CAP((uintptr_t)ARG2(__VA_ARGS__, 0,0,0,0,0,0,0,0)), PASS_CAP(ARG3(__VA_ARGS__, 0,0,0,0,0,0,0,0))
#else
#define ARG_ASSERTS(...)
// On a split file, we pass the caps separately from the
#define MARSHALL_ARGUMENTSH(...) NTH_NCAP(0, __VA_ARGS__), NTH_NCAP(1, __VA_ARGS__), NTH_NCAP(2, __VA_ARGS__), NTH_NCAP(3, __VA_ARGS__), \
                                PASS_CAP(NTH_CAP(0, __VA_ARGS__)), PASS_CAP(NTH_CAP(1, __VA_ARGS__)), PASS_CAP(NTH_CAP(2, __VA_ARGS__)), PASS_CAP(NTH_CAP(3, __VA_ARGS__))\

#endif
#define MARSHALL_ARGUMENTS(...) IF_ELSE(HAS_ARGS(__VA_ARGS__))(MARSHALL_ARGUMENTSH(__VA_ARGS__))(0,0,0,0,NULL,NULL,NULL,NULL)

#endif //CHERIOS_MARSHAL_ARGS_H
