/*-
 * Copyright (c) 2013-2015 Robert N. M. Watson
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

#ifndef _MIPS_INCLUDE_CHERIC_H_
#define	_MIPS_INCLUDE_CHERIC_H_

#define HW_SYNC __asm__ __volatile__ ("sync":::"memory")

#ifndef __ASSEMBLY__

/*
 * Canonical C-language representation of a capability.
 */
typedef __capability void * capability;
typedef __capability const void *  const_capability;
typedef capability sealing_cap;
typedef unsigned int stype;

#include "cdefs.h"

// TODO resolve PR#8 to do this properly. Some appears to already have been done
// TODO now we are picking up the compiler headers, we can hopefully pickup a lot of this from cheri.h
#include "stdint.h"

#endif

#include "platform.h"

#ifndef __ASSEMBLY__
/*
 * Useful integer type names that we can't pick up from the compile-time
 * environment.
 */

/*
 * Provide more convenient names for useful qualifiers from gcc/clang.
 */
#define	__packed__	__attribute__ ((packed))

/*
 * Useful integer type names that we can't pick up from the compile-time
 * environment.
 */
typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef long		quad_t;
typedef long		ptrdiff_t;
typedef unsigned long	u_long;
typedef unsigned long	u_quad_t;
typedef unsigned long	caddr_t;

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

#define define_intypes(size)                                \
typedef int ## size ## _t  int_least ## size ## _t;         \
typedef uint ## size ## _t  uint_least ## size ## _t;       \
typedef int ## size ## _t  int_fast ## size ## _t;          \
typedef uint ## size ## _t  uint_fast ## size ## _t;

#define INT_SIZES(ITEM) ITEM(8) ITEM(16) ITEM(32) ITEM(64)

INT_SIZES(define_intypes)

#define	NBBY		8	/* Number of bits per byte. */
#ifdef __cplusplus
#define	NULL		nullptr
#else
#define	NULL		((void *)0)
#endif

#define _safe __attribute__((temporal_safe))

#if(UNSAFE_STACKS_OFF)
#define _unsafe
#else
#define _unsafe __attribute__((temporal_unsafe))
#endif

typedef struct {
    size_t length;
    size_t mask;
} precision_rounded_length;

// TODO this is basically what cram/crap does.
static inline precision_rounded_length round_cheri_length(size_t length) {
    if(length < SMALL_OBJECT_THRESHOLD) return (precision_rounded_length){.length = length, .mask = 0};
    size_t mask = length >> (LARGE_PRECISION-1);
    mask++; // to avoid edge case where rounding length would actually change exponent
    mask |= mask >> 1L;
    mask |= mask >> 2L;
    mask |= mask >> 4L;
    mask |= mask >> 8L;
    mask |= mask >> 16L;
    mask |= mask >> 32L;
    size_t rounded_length = (length + mask) & ~mask;
    return (precision_rounded_length){.length = rounded_length, .mask = mask};
}

#define SUF_8  "b"
#define SUF_16 "h"
#define SUF_32 "w"
#define SUF_64 "d"
#define SUF_c  "c"
#define SUF_256 "c"

#define OUT_8   "r"
#define OUT_16  "r"
#define OUT_32  "r"
#define OUT_64  "r"
#define OUT_c   "C"
#define OUT_256 "C"

#define OUT_8i   "i"
#define OUT_16i  "i"
#define OUT_32i  "i"
#define OUT_64i  "i"

#define CTYPE_8 uint8_t
#define CTYPE_16	uint16_t
#define CTYPE_32	uint32_t
#define CTYPE_64 uint64_t
#define CTYPE_c	capability

#define OUT(type) "=" OUT_ ## type
#define CLOBOUT(type) "=&" OUT_ ## type
#define IN(type) OUT_ ## type
#define INOUT(type) "+" OUT_ ## type
#define CTYPE(type) CTYPE_ ## type

/*
 * Programmer-friendly macros for CHERI-aware C code -- requires use of
 * CHERI-aware Clang/LLVM, and full CP2 context switching, so not yet usable
 * in the kernel.
 */
#define	cheri_getlen(x)		__builtin_cheri_length_get(		\
				    __DECONST(capability, (x)))
#define	cheri_getbase(x)	__builtin_cheri_base_get(		\
				    __DECONST(capability, (x)))
#define cheri_gettop(x) (cheri_getbase(x) + cheri_getlen(x))
#define	cheri_getoffset(x)	__builtin_cheri_offset_get(		\
				    __DECONST(capability, (x)))
#define	cheri_getperm(x)	__builtin_cheri_perms_get(		\
				    __DECONST(capability, (x)))
#define	cheri_getsealed(x)	__builtin_cheri_sealed_get(		\
				    __DECONST(capability, (x)))
#define	cheri_gettag(x)		__builtin_cheri_tag_get(		\
				    __DECONST(capability, (x)))
#define	cheri_gettype(x)	__builtin_cheri_type_get(		\
				    __DECONST(capability, (x)))
#define	cheri_andperm(x, y)	__builtin_cheri_perms_and(		\
				    __DECONST(capability, (x)), (y))
#define	cheri_cleartag(x)	__builtin_cheri_tag_clear(		\
				    __DECONST(capability, (x)))
#define	cheri_incoffset(x, y)	__builtin_cheri_offset_increment(	\
				    __DECONST(capability, (x)), (y))
#define	cheri_setoffset(x, y)	__builtin_cheri_offset_set(		\
				    __DECONST(capability, (x)), (y))

#define	cheri_seal(x, y)	__builtin_cheri_seal(		 \
				    __DECONST(capability, (x)), \
				    __DECONST(capability, (y)))
#define	cheri_unseal(x, y)	__builtin_cheri_unseal(		 \
				    __DECONST(capability, (x)), \
				    __DECONST(capability, (y)))

#define cheri_getcursor(x) (__builtin_cheri_address_get(x))
#define cheri_setcursor(x,y) (__builtin_cheri_address_set(x, y))

#define cheri_get_low_ptr_bits(X,M) (__builtin_cheri_address_get((capability)X) & (M))
#define cheri_clear_low_ptr_bits(X,M) ((X) &~((M)))

//#define cheri_get_low_ptr_bits(ptr, mask)                                      \
//  __cheri_get_low_ptr_bits((uintptr_t)(ptr), __static_assert_sensible_low_bits(mask))

//#define cheri_set_low_ptr_bits(ptr, bits)                                      \
//  __cheri_set_low_ptr_bits((uintptr_t)(ptr), __runtime_assert_sensible_low_bits(bits))

//#define cheri_clear_low_ptr_bits(ptr, mask)                                    \
//__cheri_clear_low_ptr_bits((uintptr_t)(ptr), __static_assert_sensible_low_bits(mask))

/* TODO: Wrap these

BUILTIN(__builtin_cheri_flags_set, "v*mvC*mz", "nc")
BUILTIN(__builtin_cheri_seal_entry, "v*mvC*m", "nc")
BUILTIN(__builtin_cheri_flags_get, "zvC*m", "nc")
BUILTIN(__builtin_cheri_conditional_seal, "v*mvC*mvC*m", "nc")
BUILTIN(__builtin_cheri_type_check, "vvC*mvC*m", "nc")
BUILTIN(__builtin_cheri_perms_check, "vvC*mCz", "nc")
BUILTIN(__builtin_cheri_subset_test, "bvC*mvC*m", "nc")
BUILTIN(__builtin_cheri_callback_create, "v.", "nct")
BUILTIN(__builtin_cheri_cap_load_tags, "zvC*m", "n")
BUILTIN(__builtin_cheri_round_representable_length, "zz", "nc")
BUILTIN(__builtin_cheri_representable_alignment_mask, "zz", "nc")
BUILTIN(__builtin_cheri_cap_build, "v*mvC*mvC*m", "nc")
BUILTIN(__builtin_cheri_cap_type_copy, "v*mvC*mvC*m", "nc")

 */

#define	cheri_getdefault()	__builtin_cheri_global_data_get()
#define	cheri_getpcc()		__builtin_cheri_program_counter_get()
#define	cheri_getstack()	__builtin_cheri_stack_get()

#define	cheri_local(c)		cheri_andperm((c), ~CHERI_PERM_GLOBAL)

#define	cheri_setbounds(x, y)	__builtin_cheri_bounds_set(		\
				    __DECONST(capability, (x)), (y))

#define	cheri_setbounds_exact(x, y)	__builtin_cheri_bounds_set_exact(		\
				    __DECONST(capability, (x)), (y))


/* Names for permission bits */
#define CHERI_PERM_GLOBAL		(1 <<  0)
#define CHERI_PERM_EXECUTE		(1 <<  1)
#define CHERI_PERM_LOAD			(1 <<  2)
#define CHERI_PERM_STORE		(1 <<  3)
#define CHERI_PERM_LOAD_CAP		(1 <<  4)
#define CHERI_PERM_STORE_CAP		(1 <<  5)
#define CHERI_PERM_STORE_LOCAL_CAP	(1 <<  6)
#define CHERI_PERM_SEAL			(1 <<  7)
#define CHERI_PERM_CCALL		(1 << 8)
#define CHERI_PERM_UNSEAL		(1 << 9)
#define CHERI_PERM_ACCESS_SYS_REGS	(1 << 10)
#define CHERI_PERM_SOFT_1		(1 << 15)
#define CHERI_PERM_SOFT_2		(1 << 16)
#define CHERI_PERM_SOFT_3		(1 << 17)
#define CHERI_PERM_SOFT_4		(1 << 18)
#define CHERI_PERM_ALL		 	((1 << (11 + U_PERM_BITS)) - 1)

#define READ_ONLY(X) __DECONST(capability, cheri_andperm(X, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP))
/*
 * Two variations on cheri_ptr() based on whether we are looking for a code or
 * data capability.  The compiler's use of CFromPtr will be with respect to
 * $c0 or $pcc depending on the type of the pointer derived, so we need to use
 * types to differentiate the two versions at compile time.  We don't provide
 * the full set of function variations for code pointers as they haven't
 * proven necessary as yet.
 *
 * XXXRW: Ideally, casting via a function pointer would cause the compiler to
 * derive the capability using CFromPtr on $pcc rather than on $c0.  This
 * appears not currently to be the case, so manually derive using
 * cheri_getpcc() for now.
 */

typedef intptr_t er_t;

#define ERROR_T(T) T ## _or_er_t
#define DEC_ERROR_T(T) typedef union ERROR_T(T) {T val; er_t er;} ERROR_T(T)
#define MAKE_ER(T, code) (ERROR_T(T)){.er = (er_t)code}
#define MAKE_VALID(T, valid) (ERROR_T(T)){.val = (valid)}
#define IS_VALID(error_or_valid) cheri_gettag(error_or_valid.val)
#define ER_T_FROM_CAP(T, v) MAKE_VALID(T, v)

#define cheri_unseal_2(cap, sealing_cap) \
    ((cheri_gettag(cap) == 0 || (unsigned long)cheri_gettype(cap) != cheri_getcursor(sealing_cap)) ? NULL : cheri_unseal(cap, sealing_cap))


#define CHERI_PRINT_PTR(ptr)						\
	printf("%s: " #ptr " b:%016jx l:%016zx o:%jx\n", __func__,	\
	   cheri_getbase((const __capability void *)(ptr)),		\
	   cheri_getlen((const __capability void *)(ptr)),		\
	   cheri_getoffset((const __capability void *)(ptr)))

#define CHERI_PRINT_CAP(cap)						\
	printf("%-20s: %-16s t:%lx s:%lx p:%08jx "			\
	       "b:%016jx l:%016zx o:%jx c:%016jx type:%lx\n",				\
	   __func__,							\
	   #cap,							\
       (unsigned long)cheri_gettag(cap),						\
       (unsigned long)cheri_getsealed(cap),					\
	   cheri_getperm(cap),						\
	   cheri_getbase(cap),						\
	   cheri_getlen(cap),						\
	   cheri_getoffset(cap),						\
		cheri_getcursor(cap),				\
	   cheri_gettype(cap))

#define CHERI_PRINT_CAP_LITE(cap)					\
	printf("t:%x s:%x b:0x%16jx l:0x%16zx o:0x%jx",			\
	   cheri_gettag(cap),						\
	   cheri_getsealed(cap),					\
	   cheri_getbase(cap),						\
	   cheri_getlen(cap),						\
	   cheri_getoffset(cap))

#define CHERI_ELEM(cap, idx)						\
	cheri_setbounds(cap + idx, sizeof(cap[0]))

#define VCAP_X CHERI_PERM_EXECUTE
#define VCAP_R CHERI_PERM_LOAD
#define VCAP_W CHERI_PERM_STORE
#define VCAP_RW (VCAP_R | VCAP_W)

static inline int VCAP_I(const void * cap, size_t len, unsigned flags, u64 sealed) {
	if(!cheri_gettag(cap)) {
		return 0;
	}
	if(cheri_getsealed(cap) != sealed) {
		return 0;
	}
	if(cheri_getlen(cap) < len) {
		return 0;
	}
	if((cheri_getperm(cap) & flags) != flags) {
		return 0;
	}
	return 1;
}

static inline int VCAP(const void * cap, size_t len, unsigned flags) {
	return VCAP_I(cap, len, flags, 0);
}

static inline int VCAPS(const void * cap, size_t len, unsigned flags) {
	return VCAP_I(cap, len, flags, 1);
}

// A better NULL check if you have almighty caps floating around
#define CAP_NULL(X) (((unsigned long)(X) == 0) && !cheri_gettag(X))

#if defined(__cplusplus)
#define C_REGCLASS
#else
#define C_REGCLASS register
#endif

#endif // ASSEMBLY

#endif /* _MIPS_INCLUDE_CHERIC_H_ */
