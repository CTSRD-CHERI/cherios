/*-
 * Copyright (c) 2001, 2002 Mike Barcroft <mike@FreeBSD.org>
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: src/sys/i386/include/_stdint.h,v 1.2 2004/05/18 16:04:57 stefanf
 * $FreeBSD$
 */

#ifndef _MACHINE__STDINT_H_
#define	_MACHINE__STDINT_H_

/*
 * Useful integer type names that we can't pick up from the compile-time
 * environment.
 */
typedef __INT8_TYPE__	int8_t;
typedef __UINT8_TYPE__	uint8_t;

typedef __INT16_TYPE__	int16_t;
typedef __UINT16_TYPE__	uint16_t;

typedef __INT32_TYPE__	int32_t;
typedef __UINT32_TYPE__	uint32_t;

typedef __INT64_TYPE__	int64_t;
typedef __UINT64_TYPE__	uint64_t;
typedef __UINT64_TYPE__	uintmax_t;

typedef __INTMAX_TYPE__	intmax_t;

typedef __PTRDIFF_TYPE__	ptrdiff_t;

typedef __INTPTR_TYPE__	intptr_t;
typedef __UINTPTR_TYPE__	uintptr_t;

typedef __INT64_TYPE__	quad_t;
typedef __UINT64_TYPE__	u_quad_t;

typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned long	u_long;
typedef unsigned int	u_int;

/*
 * XXX On CHERI, this should probably be vaddr_t, but that seems to post-date
 * the CheriOS branch of LLVM.
 */
typedef unsigned long 	caddr_t;

typedef u_long		ulong;
typedef u_char		uchar;
typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
typedef uint16_t	__u16;
typedef uint32_t	__u32;
typedef uint8_t		u_int8_t;
typedef uint16_t	u_int16_t;
typedef uint32_t	u_int32_t;
typedef uint64_t	u_int64_t;

#define define_intypes(size)                                       \
typedef __INT_LEAST ## size ## _TYPE__  int_least ## size ## _t;   \
typedef __UINT_LEAST ## size ## _TYPE__ uint_least ## size ## _t;  \
typedef __INT_FAST ## size ## _TYPE__	int_fast ## size ## _t;    \
typedef __UINT_FAST ## size ## _TYPE__  uint_fast ## size ## _t;

#define INT_SIZES(ITEM) ITEM(8) ITEM(16) ITEM(32) ITEM(64)

INT_SIZES(define_intypes)

#undef define_inttypes
#undef INT_SIZES


#if !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS)

#define	INT8_C(c)		(c)
#define	INT16_C(c)		(c)
#define	INT32_C(c)		(c)

#define	UINT8_C(c)		(c)
#define	UINT16_C(c)		(c)
#define	UINT32_C(c)		(c ## U)

#ifdef __mips_n64
#define	INT64_C(c)		(c ## L)
#define	UINT64_C(c)		(c ## UL)
#else
#define	INT64_C(c)		(c ## LL)
#define	UINT64_C(c)		(c ## ULL)
#endif

#define	INTMAX_C(c)		INT64_C(c)
#define	UINTMAX_C(c)		UINT64_C(c)

#endif /* !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS) */

#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)

#ifndef __INT64_C
#ifdef __mips_n64
#define __INT64_C(c)              (c ## L)
#define __UINT64_C(c)             (c ## UL)
#else
#define __INT64_C(c)              (c ## LL)
#define __UINT64_C(c)             (c ## ULL)
#endif
#endif

/*
 * ISO/IEC 9899:1999
 * 7.18.2.1 Limits of exact-width integer types
 */
/* Minimum values of exact-width signed integer types. */
#define	INT8_MIN	(-__INT8_MAX__-1)
#define	INT16_MIN	(-__INT16_MAX__-1)
#define	INT32_MIN	(-__INT32_MAX__-1)
#define	INT64_MIN	(-__INT64_MAX__-1)

/* Maximum values of exact-width signed integer types. */
#define	INT8_MAX	__INT8_MAX__
#define	INT16_MAX	__INT16_MAX__
#define	INT32_MAX	__INT32_MAX__
#define	INT64_MAX	__INT64_MAX__

/* Maximum values of exact-width unsigned integer types. */
#define	UINT8_MAX	__UINT8_MAX__
#define	UINT16_MAX	__UINT16_MAX__
#define	UINT32_MAX	__UINT32_MAX__
#define	UINT64_MAX	__UINT64_MAX__

/*
 * ISO/IEC 9899:1999
 * 7.18.2.2  Limits of minimum-width integer types
 */
/* Minimum values of minimum-width signed integer types. */
#define	INT_LEAST8_MIN	(-__INT_LEAST8_MAX__-1)
#define	INT_LEAST16_MIN	(-__INT_LEAST16_MAX__-1)
#define	INT_LEAST32_MIN	(-__INT_LEAST32_MAX__-1)
#define	INT_LEAST64_MIN	(-__INT_LEAST64_MAX__-1)

/* Maximum values of minimum-width signed integer types. */
#define	INT_LEAST8_MAX	__INT_LEAST8_MAX__
#define	INT_LEAST16_MAX	__INT_LEAST16_MAX__
#define	INT_LEAST32_MAX	__INT_LEAST32_MAX__
#define	INT_LEAST64_MAX	__INT_LEAST64_MAX__

/* Maximum values of minimum-width unsigned integer types. */
#define	UINT_LEAST8_MAX	 __UINT_LEAST8_MAX__
#define	UINT_LEAST16_MAX __UINT_LEAST16_MAX__
#define	UINT_LEAST32_MAX __UINT_LEAST32_MAX__
#define	UINT_LEAST64_MAX __UINT_LEAST64_MAX__

/*
 * ISO/IEC 9899:1999
 * 7.18.2.3  Limits of fastest minimum-width integer types
 */
/* Minimum values of fastest minimum-width signed integer types. */
#define	INT_FAST8_MIN	(-__INT_FAST8_MAX__-1)
#define	INT_FAST16_MIN	(-__INT_FAST16_MAX__-1)
#define	INT_FAST32_MIN	(-__INT_FAST32_MAX__-1)
#define	INT_FAST64_MIN	(-__INT_FAST64_MAX__-1)

/* Maximum values of fastest minimum-width signed integer types. */
#define	INT_FAST8_MAX	__INT_FAST8_MAX__
#define	INT_FAST16_MAX	__INT_FAST16_MAX__
#define	INT_FAST32_MAX	__INT_FAST32_MAX__
#define	INT_FAST64_MAX	__INT_FAST64_MAX__

/* Maximum values of fastest minimum-width unsigned integer types. */
#define	UINT_FAST8_MAX	__UINT_FAST8_MAX__
#define	UINT_FAST16_MAX	__UINT_FAST16_MAX__
#define	UINT_FAST32_MAX	__UINT_FAST32_MAX__
#define	UINT_FAST64_MAX	__UINT_FAST64_MAX__

/*
 * ISO/IEC 9899:1999
 * 7.18.2.4  Limits of integer types capable of holding object pointers
 */
#define	INTPTR_MIN	(-__INTPTR_MAX__-1)
#define	INTPTR_MAX	__INTPTR_MAX__
#define	UINTPTR_MAX	__UINTPTR_MAX__

/*
 * ISO/IEC 9899:1999
 * 7.18.2.5  Limits of greatest-width integer types
 */
#define	INTMAX_MIN	(-__INTMAX_MAX__-1)
#define	INTMAX_MAX	__INTMAX_MAX__
#define	UINTMAX_MAX	__UINTMAX_MAX__

/*
 * ISO/IEC 9899:1999
 * 7.18.3  Limits of other integer types
 */
#define	PTRDIFF_MIN	(-__PTRDIFF_MAX__-1)
#define	PTRDIFF_MAX	__PTRDIFF_MAX__

/* Limit of size_t. */
#define	SIZE_MAX	__SIZE_MAX__

/* Limits of sig_atomic_t. */
#define	SIG_ATOMIC_MIN	(-__SIG_ATOMIC_MAX-1)
#define	SIG_ATOMIC_MAX	__SIG_ATOMIC_MAX__

/* Limits of wint_t. */
#define	WINT_MIN	(-__WINT_MAX__-1)
#define	WINT_MAX	__WINT_MAX__

#endif /* !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS) */

#endif /* !_MACHINE__STDINT_H_ */

