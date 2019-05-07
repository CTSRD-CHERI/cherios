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
#define MAKE_SIG(X) MS_HELP X
#define MAKE_ARG_LIST(X) MAL_HELP(MAL_start, EVAL5 X)
#define MAKE_ARG_LIST_APPEND(X) MAL_HELP(MAL_start_c, EVAL5 X)

#define LIST_TOTAL_CB(item, ...) 1 +
#define LIST_LENGTH(L) (L(LIST_TOTAL_CB) 0)

#define MAKE_CTR(NAME) struct _CTR_TYPE_ ## NAME ## _start {char ofsize[__COUNTER__+1];};
#define CTR(NAME)       (__COUNTER__  - sizeof(struct _CTR_TYPE_ ## NAME ## _start))

#define STRINGIFY(X) #X
#define EVAL(...)  EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL2(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL3(...) EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define EVAL4(...) EVAL5(EVAL5(EVAL5(__VA_ARGS__)))
#define EVAL5(...) __VA_ARGS__

#define EMPTY(...)
#define DEFER(...) __VA_ARGS__ EMPTY()

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

#define MAL_HELP(F, ...) EVAL(SELECT_REC(MAL_void, F, __VA_ARGS__))

#define MS_void(X)  void

#define MS_base(X) X
#define MS_rec_ind() MS_rec
#define MS_rec(name, typenext, ...) name, typenext DEFER(SELECT_REC_IND)()(MS_base, DEFER(MS_rec_ind)(), __VA_ARGS__)
#define MS_start(type, ...) type DEFER(SELECT_REC_IND)()(MS_base, DEFER(MS_rec_ind)(), __VA_ARGS__)
#define MS_HELP(...) (EVAL(SELECT_REC(MS_void, MS_start, __VA_ARGS__)))

// Blows up compiler with an integer. Use to get the size / offsets of structs ;)
#define KABOOM_INT(X) char (*__kaboom)[X] = 1;

#endif //CHERIOS_MACROUTILS_H
