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

// I have not added a yield nop to RISCV
#define HW_YIELD
#define HW_TRACE_ON __asm__ __volatile__ ("slti zero, zero, 0x1b");
#define HW_TRACE_OFF __asm__ __volatile__ ("slti zero, zero, 0x1e");
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


#define RISCV_SPECIAL_PCC 0
#define RISCV_SPECIAL_DDC 1

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

#define SET_FUNC(S, F)                              \
__asm (".weak " # S";"                              \
       "lui t0, %%captab_call_hi(" #S ");"          \
       "cincoffset ct0, c3, t0;"                    \
       "csc %[arg], %%captab_call_lo(" #S ")(ct0)"  \
       ::[arg]"C"(F):"memory","t0","ct0")

#define SET_SYM(S, V)                               \
__asm (".weak " # S";"                              \
       "lui t0, %%captab_hi(" #S ");"               \
       "cincoffset ct0, c3, t0;"                    \
       "csc %[arg], %%captab_lo(" #S ")(ct0)"       \
       ::[arg]"C"(V):"memory","t0","ct0")

#define SET_TLS_SYM(S, V)                           \
__asm (".weak " # S";"                              \
       "lui t0, %%captab_tls_hi(" #S ");"           \
       "cincoffset ct0, c31, t0;"                   \
       "csc %[arg], %%captab_tls_lo(" #S ")(ct0)"   \
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
            capability cf_c25, cf_c26, cf_c27, cf_c28, cf_c29, cf_c30;
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
        };
    };

    capability cf_idc;
    capability cf_default;
    capability cf_pcc;
} reg_frame_t;

#endif // ASSEMBLY

#define CHERI_FRAME_SIZE (32 * CAP_SIZE)
#define FRAME_idc_OFFSET       (MIPS_FRAME_SIZE + (26 * CAP_SIZE))
#define FRAME_pcc_OFFSET       (MIPS_FRAME_SIZE + (27 * CAP_SIZE))

#define NANO_KSEG 0x80000000

#endif //CHERIOS_RISV_H
