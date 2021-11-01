/*-
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2017 Lawrence Esswood
 * Copyright (c) 2016 SRI International
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

#ifndef _MATH_H_
#define _MATH_H_

#include "math_utils.h"
#include "cdefs.h"
#include "cheric.h"

#ifndef __ASSEMBLY__

typedef struct {
    long int quot;
    long int rem;
} ldiv_t;

typedef struct {
	int quot;
	int rem;
} div_t;

typedef ldiv_t lldiv_t;

__BEGIN_DECLS

static long labs(long j) {
    return(j < 0 ? -j : j);
}

static long long llabs(long long j) {
    return labs(j);
}

#define DIV_BODY 			\
    r.quot = num / denom;	\
	r.rem = num % denom;	\
							\
	if (num >= 0 && r.rem < 0) {\
		r.quot++;			\
		r.rem -= denom;		\
	}						\
	return (r);

static div_t div(int num, int denom) {
    div_t r;
	DIV_BODY
}

static ldiv_t ldiv(long num, long denom) {
	ldiv_t r;
	DIV_BODY
}

#undef DIV_BODY

static lldiv_t lldiv(long long num, long long denom) {
	return ldiv(num ,denom);
}


// Floating point stuff. Mostly all unimplemented

typedef float float_t;
typedef double double_t;

// TODO
#define UNIMPLEMENTED_MACRO (TRAP,0)
#define fpclassify(arg) UNIMPLEMENTED_MACRO
#define signbit(X) (X < 0)
#define isfinite(arg) UNIMPLEMENTED_MACRO
#define isinf(X) UNIMPLEMENTED_MACRO
#define isnan(X) UNIMPLEMENTED_MACRO
#define isnormal(X) UNIMPLEMENTED_MACRO
#define isgreater(x, y) UNIMPLEMENTED_MACRO
#define isgreaterequal(x, y) UNIMPLEMENTED_MACRO
#define isless(x, y) UNIMPLEMENTED_MACRO
#define islessequal(x, y) UNIMPLEMENTED_MACRO
#define islessgreater(x, y) UNIMPLEMENTED_MACRO
#define isunordered(x, y) UNIMPLEMENTED_MACRO

#define INFINITY UNIMPLEMENTED_MACRO

#define FP_NORMAL		0
#define FP_SUBNORMAL	1
#define FP_ZERO			2
#define FP_INFINITE		3
#define FP_NAN			4

// One arg
#define DEFINE_BUILTIN_F_T_1(Name, T, Suffix, ...) static T Name ## Suffix(T x) {return (__builtin_## Name ## Suffix(x));}
// Two args
#define DEFINE_BUILTIN_F_T_2(Name, T, Suffix, ...) static T Name ## Suffix(T x, T y) {return (__builtin_## Name ## Suffix(x,y));}
// Three args
#define DEFINE_BUILTIN_F_T_3(Name, T, Suffix, ...) static T Name ## Suffix(T x, T y, T z) {return (__builtin_## Name ## Suffix(x,y, z));}
// Two args (second is a ptr)
#define DEFINE_BUILTIN_F_T_2_Star(Name, T, Suffix, ...) static T Name ## Suffix(T x, T* y) {return (__builtin_## Name ## Suffix(x,y));}
// Two args (second is any given type)
#define DEFINE_BUILTIN_F_T_1_X(Name, T, Suffix, T2) static T Name ## Suffix(T x, T2 y) {return (__builtin_## Name ## Suffix(x,y));}
// One arg, custom return type
#define DEFINE_BUILTIN_F_T_R(Name, T, Suffix, T2) static T2 Name ## Suffix(T x) {return (__builtin_## Name ## Suffix(x));}
// Three args (third is any given type)
#define DEFINE_BUILTIN_F_T_2_X(Name, T, Suffix, T2) static T Name ## Suffix(T x, T z, T2 y) {return (__builtin_## Name ## Suffix(x,z, y));}
// One argument of a given type
#define DEFINE_BUILTIN_F_T_0_X(Name, T, Suffix, T2) static T Name ## Suffix(T2 y) {return (__builtin_## Name ## Suffix(y));}

// Different widths of floats and their suffixes
#define DEFINE_BUILTIN_F_F(Name, F,...)     \
    F(Name,float,f, __VA_ARGS__)            \
    F(Name, double,,__VA_ARGS__)            \
    F(Name,long double,l,__VA_ARGS__)

// Handy wrappers
#define DEFINE_BUILTIN_F_N(Name, N) DEFINE_BUILTIN_F_F(Name, DEFINE_BUILTIN_F_T_ ## N)
#define DEFINE_BUILTIN_F(Name) DEFINE_BUILTIN_F_N(Name, 1)
#define DEFINE_BUILTIN_F_0_OtherT(Name, T2) DEFINE_BUILTIN_F_F(Name, DEFINE_BUILTIN_F_T_0_X, T2)
#define DEFINE_BUILTIN_F_OtherT(Name, T2) DEFINE_BUILTIN_F_F(Name, DEFINE_BUILTIN_F_T_1_X, T2)
#define DEFINE_BUILTIN_F_2_OtherT(Name, T2) DEFINE_BUILTIN_F_F(Name, DEFINE_BUILTIN_F_T_2_X, T2)

#define DEFINE_BUILTIN_F_2_STAR(Name) DEFINE_BUILTIN_F_F(Name, DEFINE_BUILTIN_F_T_2_Star)
#define DEFINE_BUILTIN_F_R(Name, R) DEFINE_BUILTIN_F_F(Name, DEFINE_BUILTIN_F_T_R, R)

DEFINE_BUILTIN_F(fabs)
DEFINE_BUILTIN_F(acos)
DEFINE_BUILTIN_F(asin)
DEFINE_BUILTIN_F(sin)
DEFINE_BUILTIN_F(cos)
DEFINE_BUILTIN_F(tan)
DEFINE_BUILTIN_F(atan)
DEFINE_BUILTIN_F_N(atan2,2)
DEFINE_BUILTIN_F(ceil)
DEFINE_BUILTIN_F(floor)
DEFINE_BUILTIN_F(cosh)
DEFINE_BUILTIN_F(sinh)
DEFINE_BUILTIN_F(tanh)
DEFINE_BUILTIN_F(exp)
DEFINE_BUILTIN_F_N(fmod, 2)
DEFINE_BUILTIN_F_OtherT(frexp, int*)
DEFINE_BUILTIN_F_OtherT(ldexp, int)
DEFINE_BUILTIN_F(log)
DEFINE_BUILTIN_F(log10)
DEFINE_BUILTIN_F_N(pow, 2)
DEFINE_BUILTIN_F_2_STAR(modf)
DEFINE_BUILTIN_F(sqrt)
DEFINE_BUILTIN_F(cbrt)
DEFINE_BUILTIN_F(atanh)
DEFINE_BUILTIN_F(acosh)
DEFINE_BUILTIN_F(asinh)
DEFINE_BUILTIN_F_N(copysign,2)
DEFINE_BUILTIN_F(erf)
DEFINE_BUILTIN_F(erfc)
DEFINE_BUILTIN_F(exp2)
DEFINE_BUILTIN_F(expm1)
DEFINE_BUILTIN_F_N(fdim, 2)
DEFINE_BUILTIN_F_N(fma, 3)
DEFINE_BUILTIN_F_N(fmax, 2)
DEFINE_BUILTIN_F_N(fmin, 2)
DEFINE_BUILTIN_F_N(hypot, 2)
DEFINE_BUILTIN_F(logb)
DEFINE_BUILTIN_F_R(ilogb, int)
DEFINE_BUILTIN_F(lgamma)
DEFINE_BUILTIN_F(tgamma)
DEFINE_BUILTIN_F(rint)
DEFINE_BUILTIN_F(round)
DEFINE_BUILTIN_F(trunc)
DEFINE_BUILTIN_F_R(lrint, long)
DEFINE_BUILTIN_F_R(llrint, long long)
DEFINE_BUILTIN_F_R(lround, long)
DEFINE_BUILTIN_F_R(llround, long long)
DEFINE_BUILTIN_F(log1p)
DEFINE_BUILTIN_F(log2)
DEFINE_BUILTIN_F(nearbyint)
DEFINE_BUILTIN_F_N(nextafter, 2)
DEFINE_BUILTIN_F_OtherT(nexttoward,long double)
DEFINE_BUILTIN_F_N(remainder,2)
DEFINE_BUILTIN_F_2_OtherT(remquo,int*)
DEFINE_BUILTIN_F_OtherT(scalbn, int)
DEFINE_BUILTIN_F_OtherT(scalbln, long)
DEFINE_BUILTIN_F_0_OtherT(nan, const char*)

__END_DECLS
#else // __ASEEMBLY__

#endif

#endif
