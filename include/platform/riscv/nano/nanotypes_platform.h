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

#define MAGIC_SAFE         "ori    zero, zero, 0xdd \n"

// helper macros to load in a safe way

#define ENUM_VMEM_SAFE_DEREFERENCE(location, result, edefault)  \
    __asm__ (                                                   \
        "li     %[res], %[def]               \n"                \
        "clw    %[res], 0(%[state])   \n"                       \
        MAGIC_SAFE \
    : [res]"=&r"(result)                                         \
    : [state]"C"(location),[def]"i"(edefault)                   \
    :                                                           \
    )

#define VMEM_SAFE_DEREFERENCE(var, result, type)                \
__asm__ (                                                       \
        LOAD(type)" %[res], 0(%[loc])   \n"                     \
        MAGIC_SAFE \
    : [res]INOUT(type)(result)                                  \
    : [loc]"C"(var)                                             \
    :                                                           \
    )

// Physical memory
#define PHY_PAGE_SIZE_BITS              12
#define PHY_RAM_SIZE                    (1 << 30) // gigabyte seems sensible for now
#define IO_SIZE                         0x80000000
#define TOTAL_PHY_PAGES                 ((PHY_RAM_SIZE + IO_SIZE) >> PHY_PAGE_SIZE_BITS)
// virtual memory
#define UNTRANSLATED_BITS               PHY_PAGE_SIZE_BITS

#ifndef __ASSEMBLY__

typedef struct {
    context_t victim_context;
    register_t cause;
    register_t stval;
    register_t ex_level;
} exection_cause_t;

#endif

// Even numbers are always invalid
#define VTABLE_ENTRY_FREE               (T_E_CAST (0))
#define VTABLE_ENTRY_USED               (T_E_CAST (-2))
#define VTABLE_ENTRY_TRAN               (T_E_CAST (-4))

#endif //CHERIOS_NANOTYPES_PLATFORM_H
