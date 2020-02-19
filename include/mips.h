/*-
 * Copyright (c) 2011, 2016 Robert N. M. Watson
 * Copyright (c) 2016 Hadrien Barral
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

#ifndef _CHERIOS_MIPS_H_
#define	_CHERIOS_MIPS_H_

#ifndef SMP_ENABLED
	#define SMP_CORES 1
#endif

#ifdef HARDWARE_qemu
	#define N_TLB_ENTS	32
    #define HW_TRACE_ON __asm__ __volatile__ ("li $zero, 0xbeef");
    #define HW_TRACE_OFF __asm__ __volatile__ ("li $zero, 0xdead");
	#define ASM_TRACE_ON 	"li $zero, 0xbeef\n"
	#define ASM_TRACE_OFF 	"li $zero, 0xdead\n"

    #ifdef SMP_ENABLED
    #define HW_YIELD __asm__ __volatile__ ("nop; li  $zero, 0xea1d")
    #define YIELD li  $zero, 0xea1d
    #else
    #define HW_YIELD
    #define YIELD
    #endif

#else //qemu
    #include "cheri_pic.h"

	#define N_TLB_ASSO	16
	#define N_TLB_DRCT	256
	#define N_TLB_ENTS	(N_TLB_ASSO + N_TLB_DRCT)
    #define HW_YIELD
    #define YIELD

    // Currently turns off revoke registers. Can't turn off call variant 2 or sealing mode 2 requirements
    #define CHERI_LEGACY_COMPAT // Turn off as many of the CHERI features I added as possible. Security is lost.
#endif

#define REG_SIZE    8
#define REG_SIZE_BITS 3

#define SANE_ASM    \
".set noreorder\n"  \
".set nobopt\n"     \
".set noat\n"		\
".option pic0\n"

#define HW_SYNC __asm__ __volatile__ ("sync")

#ifndef __ASSEMBLY__

/*
 * Provide more convenient names for useful qualifiers from gcc/clang.
 */
#define	__aligned__(x)	__attribute__ ((aligned(x)))
#define	__packed__	__attribute__ ((packed))

/*
 * 64-bit MIPS types.
 */
typedef unsigned long	register_t;		/* 64-bit MIPS register */
typedef unsigned long	paddr_t;		/* Physical address */
typedef unsigned long	vaddr_t;		/* Virtual address */

typedef long		ssize_t;
typedef	unsigned long	size_t;

typedef long		off_t;

/*
 * Useful integer type names that we can't pick up from the compile-time
 * environment.
 */
typedef char		int8_t;
typedef unsigned char	u_char;
typedef unsigned char	uint8_t;
typedef short		int16_t;
typedef unsigned short	u_short;
typedef unsigned short	uint16_t;
typedef int		int32_t;
typedef unsigned int	u_int;
typedef unsigned int	uint32_t;
typedef long		intmax_t;
typedef long		quad_t;
typedef long		ptrdiff_t;
typedef long		int64_t;
typedef unsigned long	u_long;
typedef unsigned long	uint64_t;
typedef	unsigned long	uintmax_t;
typedef unsigned long	u_quad_t;
typedef __uintcap_t	uintptr_t;
//typedef unsigned long	uintptr_t;
typedef __intcap_t	intptr_t;
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

#define ULONG_MAX	4294967295UL
#define UINT_MAX	ULONG_MAX

#define	NBBY		8	/* Number of bits per byte. */
#ifdef __cplusplus
#define	NULL		nullptr
#else
#define	NULL		((void *)0)
#endif

/*
 * Useful addresses on MIPS.
 */
#define MIPS_KSEG1 0xffffffffA0000000
#define MIPS_KSEG0 0xffffffff80000000
#define MIPS_VRT   0

#define MIPS_KSEG0_64 0x9800000000000000

#define NANO_KSEG MIPS_KSEG0

#define	MIPS_BEV0_EXCEPTION_VECTOR	(MIPS_KSEG0 + 0x180)
#define	MIPS_BEV0_EXCEPTION_VECTOR_PTR	((void *)MIPS_BEV0_EXCEPTION_VECTOR)

#define	MIPS_BEV0_CCALL_VECTOR		(MIPS_KSEG0 + 0x280)
#define	MIPS_BEV0_CCALL_VECTOR_PTR	((void *)MIPS_BEV0_EXCEPTION_VECTOR)

#endif

/*
 * Hard-coded MIPS interrupt numbers.
 */
#define	MIPS_CP0_INTERRUPT_TIMER	7	/* Compare register. */

/*
 * MIPS CP0 register numbers.
 */
#define	MIPS_CP0_REG_INDEX		$0
#define	MIPS_CP0_REG_RANDOM		$1
#define	MIPS_CP0_REG_ENTRYLO0		$2
#define	MIPS_CP0_REG_ENTRYLO1		$3
#define	MIPS_CP0_REG_CONTEXT		$4
#define	MIPS_CP0_REG_PAGEMASK		$5
#define	MIPS_CP0_REG_WIRED		$6
#define	MIPS_CP0_REG_RESERVED7		$7
#define	MIPS_CP0_REG_BADVADDR		$8
#define	MIPS_CP0_REG_COUNT		$9
#define	MIPS_CP0_REG_COUNTX		9
#define	MIPS_CP0_REG_ENTRYHI		$10
#define	MIPS_CP0_REG_COMPARE		$11
#define	MIPS_CP0_REG_COMPAREX		11
#define	MIPS_CP0_REG_STATUS		$12
#define	MIPS_CP0_REG_CAUSE		$13
#define	MIPS_CP0_REG_CAUSEX		13
#define	MIPS_CP0_REG_EPC		$14
#define	MIPS_CP0_REG_PRID		$15
#define	MIPS_CP0_REG_CONFIG		$16
#define	MIPS_CP0_REG_LLADDR		$17
#define	MIPS_CP0_REG_WATCHLO		$18
#define	MIPS_CP0_REG_WATCHHI		$19
#define	MIPS_CP0_REG_XCONTEXT		$20
#define	MIPS_CP0_REG_RESERVED21		$21
#define	MIPS_CP0_REG_RESERVED22		$22
#define	MIPS_CP0_REG_RESERVED23		$23
#define	MIPS_CP0_REG_RESERVED24		$24
#define	MIPS_CP0_REG_RESERVED25		$25
#define	MIPS_CP0_REG_ECC		$26
#define	MIPS_CP0_REG_CACHEERR		$27
#define	MIPS_CP0_REG_TAGLO		$28
#define	MIPS_CP0_REG_TAGHI		$29
#define	MIPS_CP0_REG_ERROREPC		$30
#define MIPS_CP0_REG_REVOKE			$30
#define MIPS_CP0_REG_REVOKE_BASE	2
#define MIPS_CP0_REG_REVOKE_BOUND	3
#define MIPS_CP0_REG_REVOKE_PERMS	4
#define	MIPS_CP0_REG_RESERVED31		$31

#define MIPS_CP0_REG_MVPControl $0, 1
#define MIPS_CP0_REG_MVPControlX 0, 1
#define MVPControl_EVP      (1 << 0)        // Enable VPEs. Use EVPE and DVPE.
#define MVPControl_VPC      (1 << 1)        // Config enable. Must not have VPE set.

#define MIPS_CP0_REG_MVPConf0 $0, 2
#define MIPS_CP0_REG_MVPConf0X 0, 2
#define MVPConf0_PTC        (0xFF)          // Total TC's - 1

#define MIPS_CP0_REG_VPEControl $1, 1
#define MIPS_CP0_REG_VPEControlX 1, 1
#define VPEControl_TargTC   0xFF            // Target for mtt and mft

#define MIPS_CP0_REG_VPEConf0   $1, 2
#define MIPS_CP0_REG_VPEConf0X   1, 2
#define VPEConf0_MVP        (1 << 1)        // Master. can mtt and mft.
#define VPEConf0_VPA        (1 << 0)        // Active. Runs if EVP.

#define MIPS_CP0_REG_TCHalt $2, 4
#define MIPS_CP0_REG_TCHaltX 2, 4
#define TCHalt_H             1

#define MIPS_CP0_REG_TCStatus $2,1
#define MIPS_CP0_REG_TCStatusX 2,1
#define TCStatus_A          (1 << 13)

#define MIPS_CP0_REG_TCRestart $2,3
#define MIPS_CP0_REG_TCRestartX 2,3

#define MIPS_CP0_REG_TCBind $2, 2
#define MIPS_CP0_REG_TCBindX 2, 2
#define TCBind_CurVPE 0xF
#define TCBind_CurTC  (0xF << 21)
#define TCBind_CurTcShift 21

#define MIPS_ENTRYHI_ASID_SHIFT 	0
#define MIPS_ENTRYHI_ASID_BITS		8
#define MIPS_ENTRYHI_VPN_SHIFT		13
#define MIPS_ENTRYHI_VPN_BITS		51

/*
 * MIPS CP0 status register fields.
 */
#define	MIPS_CP0_STATUS_IE	0x00000001
#define	MIPS_CP0_STATUS_EXL	0x00000002	/* Exception level */
#define	MIPS_CP0_STATUS_ERL	0x00000004	/* Error level */
#define	MIPS_CP0_STATUS_KSU	0x00000018	/* Ring */
#define	MIPS_CP0_STATUS_UX	0x00000020	/* 64-bit userspace */
#define	MIPS_CP0_STATUS_SX	0x00000040	/* 64-bit supervisor */
#define	MIPS_CP0_STATUS_KX	0x00000080	/* 64-bit kernel */
#define	MIPS_CP0_STATUS_IM	0x0000ff00	/* Interrupt mask */
#define	MIPS_CP0_STATUS_DE	0x00010000	/* DS: Disable parity/ECC */
#define	MIPS_CP0_STATUS_CE	0x00020000	/* DS: Disable parity/ECC */
#define	MIPS_CP0_STATUS_CH	0x00040000	/* DS: Cache hit on cache op */
#define	MIPS_CP0_STATUS_RESERVE0	0x00080000	/* DS: Reserved */
#define	MIPS_CP0_STATUS_SR	0x00100000	/* DS: Reset signal */
#define	MIPS_CP0_STATUS_TS	0x00200000	/* DS: TLB shootdown occurred */
#define	MIPS_CP0_STATUS_BEV	0x00400000	/* DS: Boot-time exc. vectors */
#define	MIPS_CP0_STATUS_RESERVE1	0x01800000	/* DS: Reserved */
#define	MIPS_CP0_STATUS_RE	0x02000000	/* Reverse-endian bit */
#define	MIPS_CP0_STATUS_FR	0x04000000	/* Additional FP registers */
#define	MIPS_CP0_STATUS_RP	0x08000000	/* Reduced power */
#define	MIPS_CP0_STATUS_CU0	0x10000000	/* Coprocessor 0 usability */
#define	MIPS_CP0_STATUS_CU1	0x20000000	/* Coprocessor 1 usability */
#define	MIPS_CP0_STATUS_CU2	0x40000000	/* Coprocessor 2 usability */
#define	MIPS_CP0_STATUS_CU3	0x80000000	/* Coprocessor 3 usability */

/*
 * Shift values to extract multi-bit status register fields.
 */
#define	MIPS_CP0_STATUS_IM_SHIFT	8	/* Interrupt mask */

/*
 * Hard-coded MIPS interrupt bits for MIPS_CP0_STATUS_IM.
 */
#define	MIPS_CP0_STATUS_IM_TIMER	(1 << MIPS_CP0_INTERRUPT_TIMER)

/*
 * MIPS CP0 cause register fields.
 */
#define	MIPS_CP0_CAUSE_RESERVE0	0x00000003	/* Reserved bits */
#define	MIPS_CP0_CAUSE_EXCCODE	0x0000007c	/* Exception code */
#define	MIPS_CP0_CAUSE_RESERVE1	0x00000080	/* Reserved bit */
#define	MIPS_CP0_CAUSE_IP	0x0000ff00	/* Interrupt pending */
#define	MIPS_CP0_CAUSE_RESERVE2	0x0fff0000	/* Reserved bits */
#define	MIPS_CP0_CAUSE_CE	0x30000000	/* Coprocessor exception */
#define	MIPS_CP0_CAUSE_RESERVE3	0x40000000	/* Reserved bit */
#define	MIPS_CP0_CAUSE_BD	0x80000000	/* Branch-delay slot */

/*
 * Shift values to extract multi-bit cause register fields.
 */
#define	MIPS_CP0_CAUSE_EXCODE_SHIFT	2	/* Exception code */
#define	MIPS_CP0_CAUSE_IP_SHIFT		8	/* Interrupt pending */
#define	MIPS_CP0_CAUSE_CE_SHIFT		28	/* Coprocessor exception */

/*
 * MIPS exception cause codes.
 */
#define	MIPS_CP0_EXCODE_INT	0	/* Interrupt */
#define	MIPS_CP0_EXCODE_TLBMOD	1	/* TLB modification exception */
#define	MIPS_CP0_EXCODE_TLBL	2	/* TLB load/fetch exception */
#define	MIPS_CP0_EXCODE_TLBS	3	/* TLB store exception */
#define	MIPS_CP0_EXCODE_ADEL	4	/* Address load/fetch exception */
#define	MIPS_CP0_EXCODE_ADES	5	/* Address store exception */
#define	MIPS_CP0_EXCODE_IBE	6	/* Bus fetch exception */
#define	MIPS_CP0_EXCODE_DBE	7	/* Bus load/store exception */
#define	MIPS_CP0_EXCODE_SYSCALL	8	/* System call exception */
#define	MIPS_CP0_EXCODE_BREAK	9	/* Breakpoint exception */
#define	MIPS_CP0_EXCODE_RI	10	/* Reserved instruction exception */
#define	MIPS_CP0_EXCODE_CPU	11	/* Coprocessor unusable exception */
#define	MIPS_CP0_EXCODE_OV	12	/* Arithmetic overflow exception */
#define	MIPS_CP0_EXCODE_TRAP	13	/* Trap exception */
#define	MIPS_CP0_EXCODE_VCEI	14	/* Virtual coherency inst. exception */
#define	MIPS_CP0_EXCODE_FPE	15	/* Floating point exception */
#define	MIPS_CP0_EXCODE_RES0	16	/* Reserved */
#define	MIPS_CP0_EXCODE_RES1	17	/* Reserved */
#define	MIPS_CP0_EXCODE_C2E	18	/* Capability coprocessor exception */
#define	MIPS_CP0_EXCODE_RES3	19	/* Reserved */
#define	MIPS_CP0_EXCODE_RES4	20	/* Reserved */
#define	MIPS_CP0_EXCODE_RES5	21	/* Reserved */
#define	MIPS_CP0_EXCODE_RES6	22	/* Reserved */
#define	MIPS_CP0_EXCODE_WATCH	23	/* Watchpoint exception */
#define	MIPS_CP0_EXCODE_RES7	24	/* Reserved */
#define	MIPS_CP0_EXCODE_RES8	25	/* Reserved */
#define	MIPS_CP0_EXCODE_RES9	26	/* Reserved */
#define	MIPS_CP0_EXCODE_RES10	27	/* Reserved */
#define	MIPS_CP0_EXCODE_RES11	28	/* Reserved */
#define	MIPS_CP0_EXCODE_RES12	29	/* Reserved */
#define	MIPS_CP0_EXCODE_RES13	30	/* Reserved */
#define	MIPS_CP0_EXCODE_VCED	31	/* Virtual coherency data exception */

#define MIPS_CP0_EXCODE_NUM 32
/*
 * Hard-coded MIPS interrupt bits from MIPS_CP0_CAUSE_IP.
 */
#define	MIPS_CP0_CAUSE_IP_TIMER		(1 << MIPS_CP0_INTERRUPT_TIMER)


#define L1_LINE_SIZE                128 // Not according to the doc, but according to john. Will be 64 for MEM128
#define L2_LINE_SIZE                128

#define L1_SIZE						(32 * 1024)

#define CACHE_L1_INDEXS             ((L1_SIZE) / L1_LINE_SIZE)

#define CACHE_L1_INST               0
#define CACHE_L1_DATA               1
#define CACHE_L2                    3
#define CACHE_L3                    2

#define CACHE_OP_INDEX_INVAL(X)         ((0b000 << 2) | X)
#define CACHE_OP_INDEX_LOAD_TAG(X)      ((0b001 << 2) | X)
#define CACHE_OP_INDEX_STORE_TAG(X)     ((0b010 << 2) | X)
#define CACHE_OP_ADDR_HIT_INVAL(X)      ((0b100 << 2) | X)
#define CACHE_OP_ADDR_FILL(X)           ((0b101 << 2) | X) // Only for L1_INST
#define CACHE_OP_ADDR_HIT_WB_INVAL(X)   ((0b101 << 2) | X) // Only for others
#define CACHE_OP_ADDR_HIT_WB(X)         ((0b110 << 2) | X)

#define CACHE_OP(Op, Off, Base) \
    __asm __volatile__("cache %[op], %[off](%[base])"::[op]"i"(Op),[off]"i"(Off),[base]"r"(Base))
/*
 * MIPS address space layout.
 */

/* Some conflict on sources for this */

#define MIPS_XKPHYS                 0x8000000000000000ULL
#define MIPS_XKPHYS_MODE_SHIFT      59ULL
#define MIPS_XKPHYS_UNCACHED        0b010ULL       // See mips run disagrees. Mips64 and other sources agree
#define MIPS_XKPHYS_CACHED_C_EW     0b110ULL       // Other sources say this is better supported
#define MIPS_XKPHYS_CACHED_NC       0b011ULL       // Mips 64 says this for cached
#define MIPS_XKPHYS_CACHED_C        0b100ULL
#define MIPS_XKPHYS_CACHED_C_UW     0b101ULL
#define MIPS_XKPHYS_UNCACHED_ACCEL  0b111ULL



#define	MIPS_XKPHYS_UNCACHED_BASE	(MIPS_XKPHYS | (MIPS_XKPHYS_UNCACHED << MIPS_XKPHYS_MODE_SHIFT))
#define	MIPS_XKPHYS_CACHED_BASE     (MIPS_XKPHYS | (MIPS_XKPHYS_CACHED_NC << MIPS_XKPHYS_MODE_SHIFT))

#ifndef __ASSEMBLY__

static inline vaddr_t
mips_phys_to_cached(paddr_t phys)
{

	return (phys | MIPS_XKPHYS_CACHED_BASE);
}

static inline vaddr_t
mips_phys_to_uncached(paddr_t phys)
{

	return (phys | MIPS_XKPHYS_UNCACHED_BASE);
}

/*
 * Endian conversion routines for use in I/O -- most Altera devices are little
 * endian, but our processor is big endian.
 */
static inline uint16_t
byteswap16(uint16_t v)
{

	return (uint16_t)((v & 0xff00) >> 8 | (v & 0xff) << 8);
}

static inline uint32_t
byteswap32(uint32_t v)
{

	return ((v & 0xff000000) >> 24 | (v & 0x00ff0000) >> 8 |
	    (v & 0x0000ff00) << 8 | (v & 0x000000ff) << 24);
}

/*
 * MIPS simple I/O routines -- arguments are virtual addresses so that the
 * caller can determine required caching properties.
 */
static inline uint8_t
mips_ioread_uint8(vaddr_t vaddr)
{
	uint8_t v;

	__asm__ __volatile__ ("lb %0, 0(%1)" : "=r" (v) : "r" (vaddr));
	return (v);
}

static inline void
mips_iowrite_uint8(vaddr_t vaddr, uint8_t v)
{

	__asm__ __volatile__ ("sb %0, 0(%1)" : : "r" (v), "r" (vaddr));
}

static inline uint32_t
mips_ioread_uint32(vaddr_t vaddr)
{
	uint32_t v;

	__asm__ __volatile__ ("lw %0, 0(%1)" : "=r" (v) : "r" (vaddr));
	return (v);
}

static inline void
mips_iowrite_uint32(vaddr_t vaddr, uint32_t v)
{

	__asm__ __volatile__ ("sw %0, 0(%1)" : : "r" (v), "r" (vaddr));
}

/*
 * Little-endian versions of 32-bit I/O routines.
 */
static inline uint32_t
mips_ioread_uint32le(vaddr_t vaddr)
{

	return (byteswap32(mips_ioread_uint32(vaddr)));
}

static inline void
mips_iowrite_uint32le(vaddr_t vaddr, uint32_t v)
{

	mips_iowrite_uint32(vaddr, byteswap32(v));
}

/*
 * Capability versions of I/O routines.
 */
static inline uint8_t
mips_cap_ioread_uint8(void * cap, size_t offset)
{
	uint8_t v;
	__asm__ __volatile__ ("clb %[v], %[offset],  0(%[cap])"
	                      : [v] "=r" (v)
	                      : [cap] "C" (cap), [offset] "r" (offset));
	return (v);
}

static inline void
mips_cap_iowrite_uint8(void * cap, size_t offset, uint8_t v)
{
	__asm__ __volatile__ ("csb %[v], %[offset],  0(%[cap])"
	              :: [cap] "C" (cap), [offset] "r" (offset), [v] "r" (v));
}

static inline uint32_t
mips_cap_ioread_uint32(void * cap, size_t offset)
{
	uint32_t v;
	__asm__ __volatile__ ("clw %[v], %[offset],  0(%[cap])"
	                      : [v] "=r" (v)
	                      : [cap] "C" (cap), [offset] "r" (offset));
	return (v);
}

static inline void
mips_cap_iowrite_uint32(void * cap, size_t offset, uint32_t v)
{
	__asm__ __volatile__ ("csw %[v], %[offset],  0(%[cap])"
	              :: [cap] "C" (cap), [offset] "r" (offset), [v] "r" (v));
}

/*
 * Capability little-endian versions of 32-bit I/O routines.
 */
static inline uint32_t
mips_cap_ioread_uint32le(void * cap, size_t offset)
{

	return (byteswap32(mips_cap_ioread_uint32(cap, offset)));
}

static inline void
mips_cap_iowrite_uint32le(void * cap, size_t offset, uint32_t v)
{

	mips_cap_iowrite_uint32(cap, offset, byteswap32(v));
}

/*
 * Data structure describing a MIPS register frame.  Assembler routines in
 * init.s know about this layout, so great care should be taken.
 */
#if 0
struct mips_frame {
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
	register_t	mf_pc;
};
#endif

#endif // ASSEMBLY

#endif /* _CHERIOS_MIPS_H_ */
