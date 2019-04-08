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

/*
 * Derive CHERI-flavor from capability size
 */

#if _MIPS_SZCAP == 256
#define _CHERI256_
	#define U_PERM_BITS 4
    #define CAP_SIZE 0x20
	#define CAP_SIZE_S "0x20"
    #define CAP_SIZE_BITS 5
	#define SMALL_PRECISION 64
	#define LARGE_PRECISION 64
    #define CAP_EXTRA_ALIGN 0
    #define SMALL_OBJECT_THRESHOLD (~0)
#elif _MIPS_SZCAP == 128
#define _CHERI128_
	#define U_PERM_BITS 4
    #define CAP_SIZE 0x10
	#define CAP_SIZE_S "0x10"
    #define CAP_SIZE_BITS 4
	#define SMALL_PRECISION 13
	#define LARGE_PRECISION	10
    #define SMALL_OBJECT_THRESHOLD  (1 << (SMALL_PRECISION)) // Can set bounds with byte align on objects less than this

#else
#error Unknown capability size
#endif

#ifdef HARDWARE_qemu
    #define CAN_SEAL_ANY 0
#else
    #define CAN_SEAL_ANY 1
#endif

#ifndef __ASSEMBLY__

#include "cdefs.h"
#include "mips.h"

#define _safe __attribute__((temporal_safe))
#define _unsafe __attribute__((temporal_unsafe))

typedef struct {
    size_t length;
    size_t mask;
} precision_rounded_length;

static precision_rounded_length round_cheri_length(size_t length) {
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

#define ADD_8_16i	"daddiu"
#define ADD_16_16i	"daddiu"
#define ADD_32_16i	"daddiu"
#define ADD_64_16i	"daddiu"
#define ADD_c_16i	"cincoffset"

#define ADD_8_64	"daddu"
#define ADD_16_64	"daddu"
#define ADD_32_64	"daddu"
#define ADD_64_64	"daddu"
#define ADD_c_64	"cincoffset"

#define CTYPE_8 uint8_t
#define CTYPE_16	uint16_t
#define CTYPE_32	uint32_t
#define CTYPE_64 uint64_t
#define CTYPE_c	capability

#define BNE_8(a,b,l,t) "bne " a ", " b ", " l
#define BNE_16(a,b,l,t) "bne " a ", " b ", " l
#define BNE_32(a,b,l,t) "bne " a ", " b ", " l
#define BNE_64(a,b,l,t) "bne " a ", " b ", " l
#define BNE_64(a,b,l,t) "bne " a ", " b ", " l
#define BNE_c(a,b,l,t) "cexeq " t ", " a ", " b "; beqz " t ", " l

#define BNE(type, a, b, label,tmp) BNE_ ## type(a,b,label,tmp)

#define ADD(type, val_type) ADD_ ## type ## _ ## val_type

#define LOADL(type)  "cll" SUF_ ## type
#define LOAD(type) "cl" SUF_ ## type
#define STOREC(type) "csc" SUF_ ## type
#define STORE(type) "cs" SUF_ ## type
#define OUT(type) "=" OUT_ ## type
#define CLOBOUT(type) "=&" OUT_ ## type
#define IN(type) OUT_ ## type
#define INOUT(type) "+" OUT_ ## type
#define CTYPE(type) CTYPE_ ## type
/*
 * Canonical C-language representation of a capability.
 */
typedef __capability void * capability;
typedef __capability const void *  const_capability;
typedef capability sealing_cap;
typedef unsigned int stype;

/*
 * Programmer-friendly macros for CHERI-aware C code -- requires use of
 * CHERI-aware Clang/LLVM, and full CP2 context switching, so not yet usable
 * in the kernel.
 */
#define	cheri_getlen(x)		__builtin_mips_cheri_get_cap_length(		\
				    __DECONST(capability, (x)))
#define	cheri_getbase(x)	__builtin_mips_cheri_get_cap_base(		\
				    __DECONST(capability, (x)))
#define cheri_gettop(x) (cheri_getbase(x) + cheri_getlen(x))
#define	cheri_getoffset(x)	__builtin_mips_cheri_cap_offset_get(		\
				    __DECONST(capability, (x)))
#define	cheri_getperm(x)	__builtin_mips_cheri_get_cap_perms(		\
				    __DECONST(capability, (x)))
#define	cheri_getsealed(x)	__builtin_mips_cheri_get_cap_sealed(		\
				    __DECONST(capability, (x)))
#define	cheri_gettag(x)		__builtin_mips_cheri_get_cap_tag(		\
				    __DECONST(capability, (x)))
#define	cheri_gettype(x)	__builtin_mips_cheri_get_cap_type(		\
				    __DECONST(capability, (x)))

#define	cheri_andperm(x, y)	__builtin_mips_cheri_and_cap_perms(		\
				    __DECONST(capability, (x)), (y))
#define	cheri_cleartag(x)	__builtin_mips_cheri_clear_cap_tag(		\
				    __DECONST(capability, (x)))
#define	cheri_incoffset(x, y)	__builtin_mips_cheri_cap_offset_increment(	\
				    __DECONST(capability, (x)), (y))
#define	cheri_setoffset(x, y)	__builtin_mips_cheri_cap_offset_set(		\
				    __DECONST(capability, (x)), (y))

#define	cheri_seal(x, y)	__builtin_mips_cheri_seal_cap(		 \
				    __DECONST(capability, (x)), \
				    __DECONST(capability, (y)))
#define	cheri_unseal(x, y)	__builtin_mips_cheri_unseal_cap(		 \
				    __DECONST(capability, (x)), \
				    __DECONST(capability, (y)))

#define	cheri_getcause()	__builtin_mips_cheri_get_cause()
#define	cheri_setcause(x)	__builtin_mips_cheri_set_cause(x)

#define	cheri_ccheckperm(c, p)	__builtin_mips_cheri_check_perms(		\
				    __DECONST(capability, (c)), (p))
#define	cheri_cchecktype(c, t)	__builtin_mips_cheri_check_type(		\
				    __DECONST(capability, (c)), (t))

#define cheri_getcursor(x) (__builtin_cheri_address_get(x))

#define cheri_get_low_ptr_bits(X,M) (__builtin_cheri_address_get(X) & (M))
#define cheri_clear_low_ptr_bits(X,M) ((X) &~((M)))

// TODO: When we update the compiler use following instead

/*
/*
 * Get the low bits defined in @p mask from the capability/pointer @p ptr.
 * @p mask must be a compile-time constant less than 31.
 * TODO: should we allow non-constant masks?
 *
 * @param ptr the uintptr_t that may have low bits sets
 * @param mask the mask for the low pointer bits to retrieve
 * @return a size_t containing the the low bits from @p ptr
 *
 * Rationale: this function is needed because extracting the low bits using a
 * bitwise-and operation returns a LHS-derived capability with the offset
 * field set to LHS.offset & mask. This is almost certainly not what the user
 * wanted since it will always compare not equal to any integer constant.
 * For example lots of mutex code uses something like `if ((x & 1) == 1)` to
 * detect if the lock is currently contented. This comparison always returns
 * false under CHERI the LHS of the == is a valid capability with offset 3 and
 * the RHS is an untagged intcap_t with offset 3.
 * See https://github.com/CTSRD-CHERI/clang/issues/189
 */
//#define cheri_get_low_ptr_bits(ptr, mask)                                      \
//  __cheri_get_low_ptr_bits((uintptr_t)(ptr), __static_assert_sensible_low_bits(mask))

/*
 * Set low bits in a uintptr_t
 *
 * @param ptr the uintptr_t that may have low bits sets
 * @param bits the value to bitwise-or with @p ptr.
 * @return a uintptr_t that has the low bits defined in @p mask set to @p bits
 *
 * @note this function is not strictly required since a plain bitwise or will
 * generally give the behaviour that is expected from other platforms but.
 * However, we can't really make the warning "-Wcheri-bitwise-operations"
 * trigger based on of the right hand side expression since it may not be a
 * compile-time constant.
 */
//#define cheri_set_low_ptr_bits(ptr, bits)                                      \
//  __cheri_set_low_ptr_bits((uintptr_t)(ptr), __runtime_assert_sensible_low_bits(bits))

/*
 * Clear the bits in @p mask from the capability/pointer @p ptr. Mask must be
 * a compile-time constant less than 31
 *
 * TODO: should we allow non-constant masks?
 *
 * @param ptr the uintptr_t that may have low bits sets
 * @param mask this is the mask for the low pointer bits, not the mask for
 * the bits that should remain set.
 * @return a uintptr_t that has the low bits defined in @p mask set to zeroes
 *
 * @note this function is not strictly required since a plain bitwise or will
 * generally give the behaviour that is expected from other platforms but.
 * However, we can't really make the warning "-Wcheri-bitwise-operations"
 * trigger based on of the right hand side expression since it may not be a
 * compile-time constant.
 *
 */
//#define cheri_clear_low_ptr_bits(ptr, mask)                                    \
//__cheri_clear_low_ptr_bits((uintptr_t)(ptr), __static_assert_sensible_low_bits(mask))

#define cheri_setcursor(x,y) (cheri_setoffset(x, y - cheri_getbase(x)))

#define	cheri_getdefault()	__builtin_mips_cheri_get_global_data_cap()
#define	cheri_getidc()		__builtin_mips_cheri_get_invoke_data_cap()
#define	cheri_getkr0c()		__builtin_mips_cheri_get_kernel_cap1()
#define	cheri_getkr1c()		__builtin_mips_cheri_get_kernel_cap2()
#define	cheri_getkcc()		__builtin_mips_cheri_get_kernel_code_cap()
#define	cheri_getkdc()		__builtin_mips_cheri_get_kernel_data_cap()
#define	cheri_getepcc()		__builtin_mips_cheri_get_exception_program_counter_cap()
#define	cheri_getpcc()		__builtin_mips_cheri_get_program_counter_cap()
#define	cheri_getstack()	__builtin_cheri_stack_get()

#define	cheri_local(c)		cheri_andperm((c), ~CHERI_PERM_GLOBAL)

#define	cheri_setbounds(x, y)	__builtin_cheri_bounds_set(		\
				    __DECONST(capability, (x)), (y))
// TODO find instrinsic
#define	cheri_setbounds_exact(x, y)	                        \
({                                                          \
capability __exact;                                         \
__asm__ ("csetboundsexact %[out], %[in], %[len]"                 \
    : [out]"=C"(__exact)                                    \
    :[in]"C"(x),[len]"r"(y):);                              \
    __exact; \
})

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
    ((cheri_gettag(cap) == 0 || cheri_gettype(cap) != cheri_getcursor(sealing_cap)) ? NULL : cheri_unseal(cap, sealing_cap))

static __inline capability
cheri_codeptr(const void *ptr, size_t len)
{
#ifdef NOTYET
	__capability void (*c)(void) = ptr;
#else
	capability c = cheri_setoffset(cheri_getpcc(),
	    (register_t)ptr);
#endif

	/* Assume CFromPtr without base set, availability of CSetBounds. */
	return (cheri_setbounds(c, len));
}

static __inline capability
cheri_codeptrperm(const void *ptr, size_t len, register_t perm)
{

	return (cheri_andperm(cheri_codeptr(ptr, len),
	    perm | CHERI_PERM_GLOBAL));
}

static __inline capability
cheri_ptr(const void *ptr, size_t len)
{

	/* Assume CFromPtr without base set, availability of CSetBounds. */
	return (cheri_setbounds((const_capability)ptr, len));
}

static __inline capability
cheri_ptrperm(const void *ptr, size_t len, register_t perm)
{

	return (cheri_andperm(cheri_ptr(ptr, len), perm | CHERI_PERM_GLOBAL));
}

static __inline capability
cheri_ptrpermoff(const void *ptr, size_t len, register_t perm, off_t off)
{

	return (cheri_setoffset(cheri_ptrperm(ptr, len, perm), off));
}

/*
 * Construct a capability suitable to describe a type identified by 'ptr';
 * set it to zero-length with the offset equal to the base.  The caller must
 * provide a root capability (in the old world order, derived from $c0, but in
 * the new world order, likely extracted from the kernel using sysarch(2)).
 *
 * The caller may wish to assert various properties about the returned
 * capability, including that CHERI_PERM_SEAL is set.
 */

#define cheri_dla(symbol, result)               \
__asm __volatile (                              \
    "lui %[res], %%hi(" #symbol ")\n"            \
    "daddiu %[res], %[res], %%lo(" #symbol ")\n" \
: [res]"=r"(result) ::)

#define SET_FUNC(S, F) __asm (".weak " # S"; cscbi %[arg], %%capcall20(" #S ")($c25)" ::[arg]"C"(F):"memory")
#define SET_SYM(S, V) __asm (".weak " # S"; cscbi %[arg], %%captab20(" #S ")($c25)" ::[arg]"C"(V):"memory")
#define SET_TLS_SYM(S, V) __asm (".weak " #S "; cscbi %[arg], %%captab_tls20(" #S ")($c26)" ::[arg]"C"(V):"memory")

static __inline capability
cheri_maketype(capability root_type, register_t type)
{
	capability c;

	c = root_type;
	c = cheri_setoffset(c, type);	/* Set type as desired. */
	c = cheri_setbounds(c, 1);	/* ISA implies length of 1. */
	c = cheri_andperm(c, CHERI_PERM_GLOBAL | CHERI_PERM_SEAL); /* Perms. */
	return (c);
}

static __inline capability
cheri_zerocap(void)
{
	return (__capability void *)0;
}

#define	cheri_getreg(x) ({						\
	__capability void *_cap;					\
	__asm __volatile ("cmove %0, $c" #x : "=C" (_cap));		\
	_cap;								\
})

#define	cheri_setreg(x, cap) do {					\
	if ((x) == 0)							\
		__asm __volatile ("cmove $c" #x ", %0" : : "C" (cap) :	\
		    "memory");						\
	else								\
		__asm __volatile ("cmove $c" #x ", %0" : : "C" (cap));  \
} while (0)

static __inline__ capability get_idc(void) {
	capability object;
	__asm__ (
	"cmove %[object], $idc \n"
	: [object]"=C" (object));
	return object;
}

static __inline__  void set_idc(capability idc) {
	__asm__ (
	"cmove $idc, %[cookie] \n"
	:: [cookie]"C" (idc));
}

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
	   cheri_gettag(cap),						\
	   cheri_getsealed(cap),					\
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

//todo: have real one in compiler
#ifdef _CHERI128_
#define	__sealable	__attribute__((aligned(0x1000)))
#else
#define	__sealable
#endif

/*
 * Register frame to be preserved on context switching. The order of
 * save/restore is very important for both reasons of correctness and security.
 * Assembler routines know about this layout, so great care should be taken.
 */
typedef struct reg_frame {
	/*
	 * General-purpose MIPS registers.
	 */
	/* No need to preserve $zero. */
	register_t	mf_at, mf_v0, mf_v1;
	register_t	mf_a0, mf_a1, mf_a2, mf_a3, mf_a4, mf_a5, mf_a6, mf_a7;
	register_t	mf_t0, mf_t1, mf_t2, mf_t3;
	register_t	mf_s0, mf_s1, mf_s2, mf_s3, mf_s4, mf_s5, mf_s6, mf_s7;
	register_t	mf_t8, mf_t9;
	/* No need to preserve $k0, $k1. */
	register_t	mf_gp, mf_sp, mf_fp, mf_ra;

	/* Multiply/divide result registers. */
	register_t	mf_hi, mf_lo;

	/* Program counter. */
	/* register_t	mf_pc; <-- this really isn't needed, and its nice to keep to 32 regs

    /* User local kernel register */
    register_t mf_user_loc;
	/*
	 * Capability registers.
	 */
	/* c0 has special properties for MIPS load/store instructions. */
	capability	cf_default;

	/*
	 * General purpose capability registers.
	 */
	capability	cf_c1, cf_c2, cf_c3, cf_c4;
	capability	cf_c5, cf_c6, cf_c7;
	capability	cf_c8, cf_c9, cf_c10, cf_c11, cf_c12;
	capability	cf_c13, cf_c14, cf_c15, cf_c16, cf_c17;
	capability	cf_c18, cf_c19, cf_c20, cf_c21, cf_c22;
	capability	cf_c23, cf_c24, cf_c25;

	/*
	 * Special-purpose capability registers that must be preserved on a
	 * user context switch.  Note that kernel registers are omitted.
	 */
	capability	cf_idc;

	/* Program counter capability. */
	capability	cf_pcc; // WARN: Must be last for restore to work


} reg_frame_t;

#endif // ASSEMBLY

#define MIPS_FRAME_SIZE        (32*REG_SIZE)
#define CHERI_CAP_FRAME_SIZE   (28 * CAP_SIZE)
#define CHERI_FRAME_SIZE       (MIPS_FRAME_SIZE + CHERI_CAP_FRAME_SIZE)
#define FRAME_C1_OFFSET         (MIPS_FRAME_SIZE + CAP_SIZE)
#define FRAME_C3_OFFSET        (MIPS_FRAME_SIZE + (3 * CAP_SIZE))
#define FRAME_a0_OFFSET        (3 * REG_RIZE)
#define FRAME_idc_OFFSET       (MIPS_FRAME_SIZE + (26 * CAP_SIZE))
#define FRAME_pcc_OFFSET       (MIPS_FRAME_SIZE + (27 * CAP_SIZE))

#endif /* _MIPS_INCLUDE_CHERIC_H_ */
