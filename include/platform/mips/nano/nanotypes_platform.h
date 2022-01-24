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

#ifndef CHERIOS_NANOTYPES_PLATFORM_H
#define CHERIOS_NANOTYPES_PLATFORM_H

// The NOP that the nanokernel treats as marking the previous instruction as being skipable

#define MAGIC_SAFE         "ori    $zero, $zero, 0xd00d            \n"

// helper macros to load in a safe way

#define ENUM_VMEM_SAFE_DEREFERENCE(location, result, edefault)  \
    __asm__ (                                                   \
        SANE_ASM                                                \
        "li     %[res], %[def]               \n"                \
        "clw    %[res], $zero, 0(%[state])   \n"                \
        MAGIC_SAFE \
    : [res]"=&r"(result)                                         \
    : [state]"C"(location),[def]"i"(edefault)                   \
    :                                                           \
    )

#define VMEM_SAFE_DEREFERENCE(var, result, type)                \
__asm__ (                                                       \
        SANE_ASM                                                \
        LOAD(type)" %[res], $zero, 0(%[loc])   \n"              \
        MAGIC_SAFE \
    : [res]INOUT(type)(result)                                  \
    : [loc]"C"(var)                                             \
    :                                                           \
    )

// Physical memory

//TODO make this dynamic
/* Page sizes etc */
/* TODO: Would like to increase this but QEMU does not seem to support a page pask not 0 */
#define PHY_PAGE_SIZE_BITS              (12)

#ifdef HARDWARE_qemu

// QEMU did not get the MALTA memory map wrong, it is just really weird.
// MALTA has a 32bit physical address space, and the upper half aliases the lower,
// apart from the 0x10000000 to 0x20000000 range in the LOWER half which is obscured by the IO hole
// the current hack works, but we should probably have the nano kernel do some re-arranging to recover the last 256MB,
// which is actually where the IO hole is, but in the upper half.
#define PHY_RAM_SIZE                    ((1L << 31) - 0x10000000)
#define RAM_PRE_IO_END                  0x10000000
#define RAM_POST_IO_START               0x20000000
#define RAM_SPLIT
#define RAM_TAGS                        0

#else // qemu

#define PHY_RAM_SIZE                    (1024 * 1024 * 1024) // 1GB
#define RAM_PRE_IO_END                  PHY_RAM_SIZE
#define RAM_POST_IO_START               0x80008000
#define RAM_TAGS                        (((((PHY_RAM_SIZE + (1024 * 1024)) / (8 * CAP_SIZE)) + (PHY_PAGE_SIZE-1)) & ~(PHY_PAGE_SIZE-1)) * 8)
#endif

#define IO_HOLE                         (RAM_POST_IO_START - RAM_PRE_IO_END)

#define TOTAL_PHY_PAGES                 ((PHY_RAM_SIZE+IO_HOLE)/PAGE_SIZE)
#define TOTAL_LOW_RAM_PAGES             ((RAM_PRE_IO_END-RAM_TAGS)/PHY_PAGE_SIZE)
#define TOTAL_IO_PAGES                  (IO_HOLE/PHY_PAGE_SIZE)
#define TOTAL_HIGH_RAM_PAGES            ((PHY_RAM_SIZE - RAM_PRE_IO_END)/PHY_PAGE_SIZE)

// virtual memory
#define UNTRANSLATED_BITS               (1 + PHY_PAGE_SIZE_BITS) /* +1 for having two PFNs per VPN */

#ifndef __ASSEMBLY__

typedef struct {
    context_t victim_context;
    register_t cause;
    register_t ccause;
    register_t badvaddr;
    register_t ex_level;
} exection_cause_t;

#endif

#define STORE_RES_STATE                 csh
#define LOAD_RES_STATE                  clh
#define STOREC_RES_STATE                csch
#define LOADL_RES_STATE                 cllh

#define LOADL_RES_LENGTH                clld
#define STOREC_RES_LENGTH               cscd

#define VTABLE_ENTRY_FREE               (T_E_CAST (0))
#define VTABLE_ENTRY_USED               (T_E_CAST (-1))
#define VTABLE_ENTRY_TRAN               (T_E_CAST (-2))

#endif //CHERIOS_NANOTYPES_PLATFORM_H
