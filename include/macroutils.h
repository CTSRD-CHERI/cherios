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

#ifndef CHERIOS_MACROUTILS_H
#define CHERIOS_MACROUTILS_H


/* e.g. where X = (int, a, int, b)
 * MAKE_SIG(X) -> (int a, int b)
 * MAKE_ARG_LIST(X) -> a, b
 * MAKE_ARG_LIST_APPEND(X) -> , a, b
 */
#define MAKE_SIG(X) (MAKE_SIG_NO_BRACE(X))
#define MAKE_SIG_NO_BRACE(X) MS_HELP X
#define MAKE_ARG_LIST(X) MAL_HELP(MAL_start, EVAL1 X)
#define MAKE_ARG_LIST_APPEND(X) MAL_HELP(MAL_start_c, EVAL1 X)

// Expand and stringify
#define X_STRINGIFY(X) STRINGIFY(X)

// Length of a macro list
#define LIST_LENGTH(L) (L(LIST_TOTAL_CB) 0)

// Wrapper for __COUNTER__ to allow multiple resets. Dont use in conjunction with naked counter

#define MAKE_CTR(NAME) struct _CTR_TYPE_ ## NAME ## _start {char ofsize[__COUNTER__+1];};
#define USE_CTR(NAME)       (__COUNTER__  - sizeof(struct _CTR_TYPE_ ## NAME ## _start))

// Blows up compiler with an integer. Use to get the size / offsets of structs ;)
#define KABOOM_INT(X) char (*__kaboom)[X] = 1;

// Usage: IF_ELSE(condition)(X)(Y)
#define IF_ELSE(condition) _IF_ELSE(BOOL(condition))

// Will map 0->0 and any other token to 1
#define BOOL(X) NOT(NOT(X))

// FOLDr(F,z,...) -> F(arg0,F((...,argn,z)...)

#define FOLDr(F,z,arg0,...) F(arg0, IF_ELSE(HAS_ARGS(__VA_ARGS__))(DEFER2(_FOLDr)()(F,z,__VA_ARGS__))(z))
#define _FOLDr() FOLDr

// FOLDr(F,z,...) -> F((...(z, arg0)...), argn)
#define FOLDl(F,z,arg0,...) IF_ELSE(HAS_ARGS(__VA_ARGS__))(DEFER2(_FOLDl)()(F,F(z,arg0),__VA_ARGS__))(F(z,arg0))
#define _FOLDl() FOLDl

// Do F(Start, __VA_ARGS__) F(Start ## 0,__VA_ARGS__) ... (F, Start ## (Steps-1), __VA_ARGS__) (limit Steps = 15)

#define FOR_RANGE(Start, Steps, F, ...) CAT(_FOR_, Steps) (Start, F, __VA_ARGS__)

// Do F(0, __VA_ARGS__) F(1,__VA_ARGS__) ... (F, N-1, __VA_ARGS__) (limit N = 15)

#define FOR(F, N, ...) FOR_RANGE (0x, N, F, __VA_ARGS__)

// For loop but takes an 4 digit hex number. Usage: FOR_BASE_16(F, Digit3, Digit2, Digit1, Digit0, ...)
#define FOR_BASE_16(...) EVAL32(_FOR_BASE_16(__VA_ARGS__))

#define _FOR_BASE_16(F, H3, H2, H1, H0, ...)                            \
    FOR_RANGE(0x, H3, _FOR_16, _FOR_16_2, _FOR_16_3, F, __VA_ARGS__)    \
    FOR_RANGE(0x ## H3, H2, _FOR_16, _FOR_16_2, F, __VA_ARGS__)         \
    FOR_RANGE(0x ## H3 ## H2, H1, _FOR_16, F, __VA_ARGS__)              \
    FOR_RANGE(0x ## H3 ## H2 ## H1, H0, F,  __VA_ARGS__)


// Whole bunch of helpers

#define __FOR_0() _FOR_0
#define __FOR_1() _FOR_1
#define __FOR_2() _FOR_2
#define __FOR_3() _FOR_3
#define __FOR_4() _FOR_4
#define __FOR_5() _FOR_5
#define __FOR_6() _FOR_6
#define __FOR_7() _FOR_7
#define __FOR_8() _FOR_8
#define __FOR_9() _FOR_9
#define __FOR_a() _FOR_a
#define __FOR_b() _FOR_b
#define __FOR_c() _FOR_c
#define __FOR_d() _FOR_d
#define __FOR_e() _FOR_e
#define __FOR_f() _FOR_f

#define _FOR_0(...)
#define _FOR_1(Start, F, ...) DEFER(__FOR_0)()(Start, F, __VA_ARGS__) F(Start ## 0, __VA_ARGS__)
#define _FOR_2(Start, F, ...) DEFER(__FOR_1)()(Start, F, __VA_ARGS__) F(Start ## 1 , __VA_ARGS__)
#define _FOR_3(Start, F, ...) DEFER(__FOR_2)()(Start, F, __VA_ARGS__) F(Start ## 2, __VA_ARGS__)
#define _FOR_4(Start, F, ...) DEFER(__FOR_3)()(Start, F, __VA_ARGS__) F(Start ## 3, __VA_ARGS__)
#define _FOR_5(Start, F, ...) DEFER(__FOR_4)()(Start, F, __VA_ARGS__) F(Start ## 4, __VA_ARGS__)
#define _FOR_6(Start, F, ...) DEFER(__FOR_5)()(Start, F, __VA_ARGS__) F(Start ## 5, __VA_ARGS__)
#define _FOR_7(Start, F, ...) DEFER(__FOR_6)()(Start, F, __VA_ARGS__) F(Start ## 6, __VA_ARGS__)
#define _FOR_8(Start, F, ...) DEFER(__FOR_7)()(Start, F, __VA_ARGS__) F(Start ## 7, __VA_ARGS__)
#define _FOR_9(Start, F, ...) DEFER(__FOR_8)()(Start, F, __VA_ARGS__) F(Start ## 8, __VA_ARGS__)
#define _FOR_a(Start, F, ...) DEFER(__FOR_9)()(Start, F, __VA_ARGS__) F(Start ## 9, __VA_ARGS__)
#define _FOR_b(Start, F, ...) DEFER(__FOR_a)()(Start, F, __VA_ARGS__) F(Start ## a, __VA_ARGS__)
#define _FOR_c(Start, F, ...) DEFER(__FOR_b)()(Start, F, __VA_ARGS__) F(Start ## b, __VA_ARGS__)
#define _FOR_d(Start, F, ...) DEFER(__FOR_c)()(Start, F, __VA_ARGS__) F(Start ## c, __VA_ARGS__)
#define _FOR_e(Start, F, ...) DEFER(__FOR_d)()(Start, F, __VA_ARGS__) F(Start ## d, __VA_ARGS__)
#define _FOR_f(Start, F, ...) DEFER(__FOR_e)()(Start, F, __VA_ARGS__) F(Start ## e, __VA_ARGS__)
#define _FOR_16(Start, F, ...) DEFER(__FOR_f)()(Start, F, __VA_ARGS__) F(Start ## f, __VA_ARGS__)
#define _FOR_16_2(Start, F, ...) DEFER(__FOR_f)()(Start, F, __VA_ARGS__) F(Start ## f, __VA_ARGS__)
#define _FOR_16_3(Start, F, ...) DEFER(__FOR_f)()(Start, F, __VA_ARGS__) F(Start ## f, __VA_ARGS__)

#define CAT(a,b) a ## b

#define NOT(x) IS_PROBE(CAT(_NOT_, x))
#define _NOT_0 PROBE()

#define IS_PROBE(...) SECOND(__VA_ARGS__, 0)
#define PROBE() ~, 1

#define SECOND(a, b, ...) b
#define FIRST(a, ...) a

#define HAS_ARGS(...) BOOL(FIRST(_END_OF_ARGUMENTS_ __VA_ARGS__)())
#define _END_OF_ARGUMENTS_() 0


#define _IF_ELSE(condition) CAT(_IF_, condition)

#define _IF_1(...) __VA_ARGS__ _IF_1_ELSE
#define _IF_0(...)             _IF_0_ELSE

#define _IF_1_ELSE(...)
#define _IF_0_ELSE(...) __VA_ARGS__

#define LIST_TOTAL_CB(item, ...) 1 +

#define STRINGIFY(X) #X

#define EVAL(...) EVAL1024(__VA_ARGS__)
#define EVAL1024(...) EVAL512(EVAL512(__VA_ARGS__))
#define EVAL512(...) EVAL256(EVAL256(__VA_ARGS__))
#define EVAL256(...) EVAL128(EVAL128(__VA_ARGS__))
#define EVAL128(...) EVAL64(EVAL64(__VA_ARGS__))
#define EVAL64(...) EVAL32(EVAL32(__VA_ARGS__))
#define EVAL32(...) EVAL16(EVAL16(__VA_ARGS__))
#define EVAL16(...) EVAL8(EVAL8(__VA_ARGS__))
#define EVAL8(...) EVAL4(EVAL4(__VA_ARGS__))
#define EVAL4(...) EVAL2(EVAL2(__VA_ARGS__))
#define EVAL2(...) EVAL1(EVAL1(__VA_ARGS__))
#define EVAL1(...) __VA_ARGS__

#define EMPTY(...)
#define DEFER(...) __VA_ARGS__ EMPTY()
#define DEFER2(...) __VA_ARGS__ EMPTY EMPTY()()

#define MULTI32(X) X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X
#define ANY_HELP(...) ANY(__VA_ARGS__)
#define ANY(_1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
     _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
     _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
     _31,_32, _33, X, ...) X
#define SELECT_REC(BASE, REC, ...) ANY_HELP(__VA_ARGS__, MULTI32(REC), BASE) (__VA_ARGS__)
#define SELECT_REC_IND() SELECT_REC

#define MAL_void(X)

#define MAL_base(name) name
#define MAL_rec_ind() MAL_rec
#define MAL_rec(name, typenext, ...) name, DEFER(SELECT_REC_IND)()(MAL_base, DEFER(MAL_rec_ind)(), __VA_ARGS__)
#define MAL_start(type, ...) DEFER(SELECT_REC_IND)()(MAL_base, DEFER(MAL_rec_ind)(), __VA_ARGS__)

#define MAL_start_c(type, ...) , DEFER(SELECT_REC_IND)()(MAL_base, DEFER(MAL_rec_ind)(), __VA_ARGS__)

#define MAL_HELP(F, ...) EVAL32(SELECT_REC(MAL_void, F, __VA_ARGS__))

#define MS_void(X)  void

#define MS_base(X) X
#define MS_rec_ind() MS_rec
#define MS_rec(name, typenext, ...) name, typenext DEFER(SELECT_REC_IND)()(MS_base, DEFER(MS_rec_ind)(), __VA_ARGS__)
#define MS_start(type, ...) type DEFER(SELECT_REC_IND)()(MS_base, DEFER(MS_rec_ind)(), __VA_ARGS__)
#define MS_HELP(...) EVAL32(SELECT_REC(MS_void, MS_start, __VA_ARGS__))

#endif //CHERIOS_MACROUTILS_H
