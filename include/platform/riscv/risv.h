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

#ifndef CHERIOS_RISV_H
#define CHERIOS_RISV_H

#define CAN_SEAL_ANY 1
#define MERGED_FILE

#define REG_SIZE    8
#define REG_SIZE_BITS 3

// TODO RISCV check these, I am just assuming they are the same as mips so copied them from there
#define _CHERI128_
#define U_PERM_BITS 4
#define CAP_SIZE 0x10
#define CAP_SIZE_S "0x10"
#define CAP_SIZE_BITS 4
#define BOTTOM_PRECISION 14
#define SMALL_PRECISION (BOTTOM_PRECISION - 1)
#define LARGE_PRECISION	(BOTTOM_PRECISION - 4)
#define SMALL_OBJECT_THRESHOLD  (1 << (SMALL_PRECISION)) // Can set bounds with byte align on objects less than this
#define CAN_SEAL_ANY 1

#define SANE_ASM

#define ASM_TRACE_ON 	"slti zero, zero, 0x1b\n"
#define ASM_TRACE_OFF 	"slti zero, zero, 0x1e\n"

// I have not added a yield nop to RISCV
#define HW_YIELD
#define HW_TRACE_ON __asm__ __volatile__ (ASM_TRACE_ON);
#define HW_TRACE_OFF __asm__ __volatile__ (ASM_TRACE_OFF);
// Note, I have chosen fence rather than fence.i here. Some consumers may want fence.i,
// especially around boot / program loading
#define HW_SYNC __asm__ __volatile__ ("fence":::"memory")
#define HW_SYNC_I __asm__ __volatile__ ("fence.i":::"memory")

// Some macros for some asm instructions

#define ASMADD_8_16i	"addi"
#define ASMADD_16_16i	"addi"
#define ASMADD_32_16i	"addi"
#define ASMADD_64_16i	"addi"
#define ASMADD_c_16i	"cincoffsetimm"

#define ASMADD_8_64	    "add"
#define ASMADD_16_64	"add"
#define ASMADD_32_64	"add"
#define ASMADD_64_64	"add"
#define ASMADD_c_64	    "cincoffset"

#define BNE_8(a,b,l,t) "bne " a ", " b ", " l
#define BNE_16(a,b,l,t) "bne " a ", " b ", " l
#define BNE_32(a,b,l,t) "bne " a ", " b ", " l
#define BNE_64(a,b,l,t) "bne " a ", " b ", " l
#define BNE_64(a,b,l,t) "bne " a ", " b ", " l
#define BNE_c(a,b,l,t) "CSetEqualExact " t ", " a ", " b "; beqz " t ", " l

#define BNE(type, a, b, label,tmp) BNE_ ## type(a,b,label,tmp)

#define ASMADD(type, val_type) ASMADD_ ## type ## _ ## val_type

#define LOADL(type)  "clr." SUF_ ## type
#define LOAD(type) "cl" SUF_ ## type
#define STOREC(type) "csc." SUF_ ## type
#define STORE(type) "cs" SUF_ ## type


#ifndef __ASSEMBLY__

/*
 * 64-bit RISCV types.
 */

typedef unsigned long	register_t;		/* 64-bit register */
typedef unsigned long	paddr_t;		/* Physical address */
typedef unsigned long	vaddr_t;		/* Virtual address */

typedef long		ssize_t;
typedef	unsigned long	size_t;

typedef long		off_t;

#define cheri_getreg(X)                                         \
    ({  capability _getreg_tmp;                                 \
        __asm("cmove %[dst], " X:[dst]"=C"(_getreg_tmp)::);     \
        _getreg_tmp;                                            \
    })

#define cheri_setreg(X, V) __asm("cmove " X ", %[src]" ::[src]"C"(V):)

#define	cheri_getidc() cheri_getreg("c31")
#define set_idc(X) cheri_setreg("c31", X)

// NOTE: Relocations cannot be used with csc as they were designed for for clc and so will target the wrong portion of
// the instruction. We could improve the relocation to know where the offset field is for different instructions,
// but it also works to just use a cincoffset for the %lo portion.

#define SET_FUNC(S, F)                              \
__asm (".weak " # S";"                              \
       "lui t0, %%captab_call_hi(" #S ");"          \
       "cincoffset ct0, c3, t0;"                    \
       "cincoffset ct0, ct0, %%captab_call_lo(" #S ");"\
       "csc %[arg], 0(ct0)"                         \
       ::[arg]"C"(F):"memory","t0","ct0")

#define SET_SYM(S, V)                               \
__asm (".weak " # S";"                              \
       "lui t0, %%captab_hi(" #S ");"               \
       "cincoffset ct0, c3, t0;"                    \
       "cincoffset ct0, ct0, %%captab_lo(" #S ");"  \
       "csc %[arg], 0(ct0)"                         \
       ::[arg]"C"(V):"memory","t0","ct0")

#define SET_TLS_SYM(S, V)                           \
__asm (".weak " #S ";"                              \
       ".type " #S ", \"tls_object\"\n"             \
       "lui t0, %%captab_tls_hi(" #S ");"           \
       "cincoffset ct0, c31, t0;"                   \
       "cincoffset ct0, ct0, %%captab_tls_lo(" #S ");"\
       "csc %[arg],0(ct0)"                          \
       ::[arg]"C"(V):"memory","t0","ct0")

#define STORE_IDC_INDEX(index, value)               \
__asm ("cincoffset ct0, c31, %[off];"               \
       "csc %[arg], 0(ct0)"                         \
       ::[arg]"C"(value),[off]"r"(index):"memory","t0","ct0");

// This is really la, not dla. But I think it will suffice.
#define cheri_dla(symbol, result)               \
__asm __volatile (                              \
    "lui %[res], %%hi(" #symbol ")\n"           \
    "addi %[res], %[res], %%lo(" #symbol ")\n"  \
: [res]"=r"(result) ::)

// Turns out it will not suffice as 32-bit addresses will end up sign extended on RISCV and boot addresses have the high
// bit set. This dla assumes that, and makes solves the linker error.
#define cheri_dla_boot(symbol, result) cheri_dla(symbol - 0x80000000, result); result += 0x80000000

#define cheri_asm_getpcc(asm_out) "1: auipcc " asm_out ", 0\n"

// TODO RISCV everything below is a dummy to get things to compile

// TODO: Should these have ABI names? I don't think ABI belongs at this level, and reg_abi.h exists.
typedef struct reg_frame {
    union {
        struct {
            capability cf_c1, cf_c2, cf_c3, cf_c4, cf_c5, cf_c6, cf_c7, cf_c8;
            capability cf_c9, cf_c10, cf_c11, cf_c12, cf_c13, cf_c14, cf_c15, cf_c16;
            capability cf_c17, cf_c18, cf_c19, cf_c20, cf_c21, cf_c22, cf_c23, cf_c24;
            capability cf_c25, cf_c26, cf_c27, cf_c28, cf_c29, cf_c30, cf_c31;
        };
        struct {
            struct {uint64_t cf_x1; uint64_t cf_x1_hi;};
            struct {uint64_t cf_x2; uint64_t cf_x2_hi;};
            struct {uint64_t cf_x3; uint64_t cf_x3_hi;};
            struct {uint64_t cf_x4; uint64_t cf_x4_hi;};
            struct {uint64_t cf_x5; uint64_t cf_x5_hi;};
            struct {uint64_t cf_x6; uint64_t cf_x6_hi;};
            struct {uint64_t cf_x7; uint64_t cf_x7_hi;};
            struct {uint64_t cf_x8; uint64_t cf_x8_hi;};
            struct {uint64_t cf_x9; uint64_t cf_x9_hi;};
            struct {uint64_t cf_x10; uint64_t cf_x10_hi;};
            struct {uint64_t cf_x11; uint64_t cf_x11_hi;};
            struct {uint64_t cf_x12; uint64_t cf_x12_hi;};
            struct {uint64_t cf_x13; uint64_t cf_x13_hi;};
            struct {uint64_t cf_x14; uint64_t cf_x14_hi;};
            struct {uint64_t cf_x15; uint64_t cf_x15_hi;};
            struct {uint64_t cf_x16; uint64_t cf_x16_hi;};
            struct {uint64_t cf_x17; uint64_t cf_x17_hi;};
            struct {uint64_t cf_x18; uint64_t cf_x18_hi;};
            struct {uint64_t cf_x19; uint64_t cf_x19_hi;};
            struct {uint64_t cf_x20; uint64_t cf_x20_hi;};
            struct {uint64_t cf_x21; uint64_t cf_x21_hi;};
            struct {uint64_t cf_x22; uint64_t cf_x22_hi;};
            struct {uint64_t cf_x23; uint64_t cf_x23_hi;};
            struct {uint64_t cf_x24; uint64_t cf_x24_hi;};
            struct {uint64_t cf_x25; uint64_t cf_x25_hi;};
            struct {uint64_t cf_x26; uint64_t cf_x26_hi;};
            struct {uint64_t cf_x27; uint64_t cf_x27_hi;};
            struct {uint64_t cf_x28; uint64_t cf_x28_hi;};
            struct {uint64_t cf_x29; uint64_t cf_x29_hi;};
            struct {uint64_t cf_x30; uint64_t cf_x30_hi;};
            struct {uint64_t cf_x31; uint64_t cf_x31_hi;};
        };
    };
    capability cf_default;
    capability cf_pcc;
} reg_frame_t;

#endif // ASSEMBLY

#define REG_FRAME_INDEX_DEFAULT 31
#define REG_FRAME_INDEX_PCC 32
#define CHERI_FRAME_SIZE (33 * CAP_SIZE)

#define RAM_START 0x80000000
#define NANO_KSEG RAM_START


#define RISCV_SATP_MODE_SHIFT   60
#define RISCV_SATP_ASID_SHIFT   44
#define RISCV_SATP_ASID_MASK    0x1FF

#define RISCV_SATP_MODE_BARE    0
#define RISCV_SATP_MODE_Sv39    8

/* XWR can have some funky meanings
X	W	R	Meaning
0	0	0	Pointer to next level of page table.
0	0	1	Read-only page.
0	1	0	Reserved for future use.
0	1	1	Read-write page.
1	0	0	Execute-only page.
1	0	1	Read-execute page.
1	1	0	Reserved for future use.
1	1	1	Read-write-execute page.
*/

#define RISCV_PTE_V             (1 << 0) // valid
#define RISCV_PTE_R             (1 << 1) // read
#define RISCV_PTE_W             (1 << 2) // write
#define RISCV_PTE_X             (1 << 3) // execute
#define RISCV_PTE_U             (1 << 4) // user
#define RISCV_PTE_G             (1 << 5) // global (ignore asid)
#define RISCV_PTE_A             (1 << 6)
#define RISCV_PTE_D             (1 << 7) // dirty bit

#define RISCV_PTE_CRG           (1ULL << 59)
#define RISCV_PTE_CRM           (1ULL << 60)
#define RISCV_PTE_CD            (1ULL << 61) // cap dirty
#define RISCV_PTE_CR            (1ULL << 62) // cap read
#define RISCV_PTE_CW            (1ULL << 63) // cap write

#define RISCV_PTE_PERM_SUB_TABLE (RISCV_PTE_V)
#define RISCV_PTE_TYPE_MASK     (RISCV_PTE_V | RISCV_PTE_X | RISCV_PTE_W | RISCV_PTE_R)

#define RISCV_PTE_PERM_ALL      (RISCV_PTE_V | RISCV_PTE_R | RISCV_PTE_W | RISCV_PTE_X | RISCV_PTE_G | \
                                 RISCV_PTE_D | RISCV_PTE_A | RISCV_PTE_CD | RISCV_PTE_CR | RISCV_PTE_CW)

#define TLB_FLAGS_DEFAULT       RISCV_PTE_PERM_ALL
// RISCV does not actually have cache attributes for its TLB (saaad).
// Just making these the same we will see what breaks.
#define TLB_FLAGS_UNCACHED      RISCV_PTE_PERM_ALL

#define RISCV_PTE_RSW_SHIFT     8       // reserved for software
#define RISCV_PTE_PFN_SHIFT     10

#define RISCV_CAUSE_INT_SHIFT       63
#define RISCV_CAUSE_MASK            0xFFFF

#define RISCV_CAUSE_LIST(ITEM)             \
    ITEM(RISCV_CAUSE_ISN_ALIGN       , 0x0)\
    ITEM(RISCV_CAUSE_ISN_ACCESS      , 0x1)\
    ITEM(RISCV_CAUSE_ISN_ILLEGAL     , 0x2)\
    ITEM(RISCV_CAUSE_BREAK           , 0x3)\
    ITEM(RISCV_CAUSE_LOAD_ALIGN      , 0x4)\
    ITEM(RISCV_CAUSE_LOAD_ACCESS     , 0x5)\
    ITEM(RISCV_CAUSE_STORE_ALIGN     , 0x6)\
    ITEM(RISCV_CAUSE_STORE_ACCESS    , 0x7)\
    ITEM(RISCV_CAUSE_CALL_U          , 0x8)\
    ITEM(RISCV_CAUSE_CALL_S          , 0x9)\
    ITEM(RISCV_CAUSE_CALL_M          , 0xb)\
    ITEM(RISCV_CAUSE_ISN_PAGE        , 0xc)\
    ITEM(RISCV_CAUSE_LOAD_PAGE       , 0xd)\
    ITEM(RISCV_CAUSE_STORE_PAGE      , 0xf)\
    ITEM(RISCV_CAUSE_CHERI           , 0x1c)\
    ITEM(RISCV_CAUSE_USER_SOFT       , 0x0 | (1ULL << RISCV_CAUSE_INT_SHIFT))\
    ITEM(RISCV_CAUSE_SUPER_SOFT      , 0x1 | (1ULL << RISCV_CAUSE_INT_SHIFT))\
    ITEM(RISCV_CAUSE_USER_TIMER      , 0x4 | (1ULL << RISCV_CAUSE_INT_SHIFT))\
    ITEM(RISCV_CAUSE_SUPER_TIMER     , 0x5 | (1ULL << RISCV_CAUSE_INT_SHIFT))\

#include "string_enums.h"

DECLARE_ENUM(riscv_cause, RISCV_CAUSE_LIST)

#define RISCV_CAUSE_EXCODE_NUM      0x10

#define RISCV_M                     0b11
#define RISCV_S                     0b01
#define RISCV_U                     0b00

#define RISCV_STATUS_MPP_SHIFT      11
#define RISCV_STATUS_SPP_SHIFT      8

#define RISCV_STATUS_SIE            (1 << 1)
#define RISCV_STATUS_MIE            (1 << 3)
#define RISCV_STATUS_MIE            (1 << 3)
#define RISCV_STATUS_MPP            (1 << 17)


#define RISCV_STATUS_SPIE           (1 << 5)

// Interrupt enable (software)
#define RISCV_MIE_SSIE              (1 << 1)
#define RISCV_MIE_MSIE              (1 << 3)
// Interrupt enable (timer)
#define RISCV_MIE_STIE              (1 << 5)
#define RISCV_MIE_MTIE              (1 << 7)
// Interrupt enable (external)
#define RISCV_MIE_SEIE              (1 << 9)
#define RISCV_MIE_MEIE              (1 << 11)
#endif //CHERIOS_RISV_H
