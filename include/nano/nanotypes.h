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

#ifndef CHERIOS_NANOTYPES_H
#define CHERIOS_NANOTYPES_H

#include "cheric.h"

#include "string_enums.h"

#define TYPE_SPACE_BITS     20
#define TYPE_SPACE          (1 << TYPE_SPACE_BITS)


// All types below this value are reserved by the nano kernel

#define NANO_TYPES         0x100

#define SYSTEM_TYPES       (TYPE_SPACE - NANO_TYPES)
#define TRES_BITFIELD_SIZE ((SYSTEM_TYPES+7)/8)

/* The types used for nano kernel objects. */
#define CONTEXT_TYPE       0x0001         // The type of contexts
#define NANO_KERNEL_TYPE   0x0002         // The type of sealed local data
#define RES_TYPE           0x0003         // The type of a reservation handle
#define TRES_TYPE          0x0004         // The type of a type reservation
#define FOUND_ENTRY_TYPE   0x0010         // The type of a foundation entry handle
#define FOUND_CERT_TYPE    0x0011         // The type of a foundation certificate handle
#define FOUND_LOCKED_TYPE  0x0010         // The type of a foundation locked message hnadle
#define VTABLE_TYPE_L0     0x0020         // The type of the top level page table
#define VTABLE_TYPE_L1     VTABLE_TYPE_L0 + 1  // The type of the L1 level page table
#define VTABLE_TYPE_L2     VTABLE_TYPE_L0 + 2  // The type of the L2 level page table

#define VTABLE_LEVELS      3              // Number of table levels


/* Size of metadata for reservations. Split into private and user data */
#define RES_PRIV_SIZE                   (0)
#define RES_META_SIZE                   (32)
#define RES_USER_SIZE                   (RES_META_SIZE - RES_PRIV_SIZE)


#define RES_LENGTH_OFFSET               16
#define RES_PID_OFFSET                  24
#define RES_STATE_OFFSET                0           // Keep zero for link/conditional
#define STORE_RES_STATE                 csb
#define LOAD_RES_STATE                  clb
#define STOREC_RES_STATE                cscb
#define LOADL_RES_STATE                 cllb
#define RES_SUBFIELD_SIZE_OFFSET        1
#define RES_SIZE_NOT_FIELD              0xFF
#define RES_SUBFIELD_BITMAP_OFFSET      2
#define RES_SUBFIELD_BITMAP_BITS        (14 * 8)


// The layout of contexts


// We will lay out our contexts like this

// FIXME We could shorten our code by re-ordering context_t
// FIXME We need to put normal registers first, then capability regs

// struct context_t {
//   reg_frame_t
//   enum state{allocated, dead} (size of a double)
//   emum exception_state{ user_handle = 1, in_exception = 2} (size of a double
//   user_cause
//   user_ccause
//   padded to a cap
//   found_id_t* foundation
//   exception_pcc       // PCC that was the exception pcc
//   exception_idc       // IDC that was the exception idc
//   exception_saved_idc // IDC to do swapping with
//};

#define CONTEXT_SIZE                    (CHERI_FRAME_SIZE + (CAP_SIZE * 5))
#define CONTEXT_OFFSET_STATE            CHERI_FRAME_SIZE

#define CONTEXT_STATE_CREATED           0
#define CONTEXT_STATE_DESTROYED         1

#define CONTEXT_OFFSET_EX_STATE         CHERI_FRAME_SIZE + REG_SIZE

#define EX_STATE_UH                     1
#define EX_STATE_EL                     2

#define CONTEXT_OFFSET_CAUSE            (CHERI_FRAME_SIZE + (2 * REG_SIZE))
#define CONTEXT_OFFSET_CCAUSE           (CHERI_FRAME_SIZE + (3 * REG_SIZE))
#define CONTEXT_OFFSET_FOUND            (CHERI_FRAME_SIZE + CAP_SIZE)
#define CONTEXT_OFFSET_EX_PCC           (CHERI_FRAME_SIZE + (2 * CAP_SIZE))
#define CONTEXT_OFFSET_EX_IDC           (CHERI_FRAME_SIZE + (3 * CAP_SIZE))
#define CONTEXT_OFFSET_EX_SAVED_IDC     (CHERI_FRAME_SIZE + (4 * CAP_SIZE))
// We never reallocate these, so its currently a huge limitation
// Create context should take an optional reservation similar to the how the kernel gets non static space
#define N_CONTEXTS                      (64)

//TODO make this dynamic
/* Page sizes etc */
#define PHY_RAM_SIZE                    (1L << 31)
#define RAM_PRE_IO_END                  0x10000000
#define RAM_POST_IO_START               0x20000000
#define IO_HOLE                         (RAM_POST_IO_START - RAM_PRE_IO_END)

/* TODO: Would like to increase this but QEMU does not seem to support a page pask not 0 */
#define PHY_PAGE_SIZE_BITS              (12)
#define PHY_PAGE_SIZE                   (1 << PHY_PAGE_SIZE_BITS)
#define TOTAL_PHY_PAGES                 ((PHY_RAM_SIZE+IO_HOLE)/PAGE_SIZE)
#define TOTAL_LOW_RAM_PAGES             (RAM_PRE_IO_END/PHY_PAGE_SIZE)
#define TOTAL_IO_PAGES                  (IO_HOLE/PHY_PAGE_SIZE)
#define TOTAL_HIGH_RAM_PAGES            ((PHY_RAM_SIZE - RAM_PRE_IO_END)/PHY_PAGE_SIZE)
#define PHY_ADDR_TO_PAGEN(addr)         ((addr >> PHY_PAGE_SIZE_BITS))


/* Physical page records */
#define PHY_PAGE_ENTRY_SIZE_BITS        (REG_SIZE_BITS + 2)
#define PHY_PAGE_ENTRY_SIZE             (1L << PHY_PAGE_ENTRY_SIZE_BITS)
#define PHY_PAGE_OFFSET_status          0
#define PHY_PAGE_OFFSET_len             REG_SIZE
#define PHY_PAGE_OFFSET_prev            (2*REG_SIZE)
#define PHY_PAGE_OFFSET_spare           (3*REG_SIZE)

/* Virtual page table records */

/* In this version we are using one physical page for each page table at each level */

#define PAGE_TABLE_BITS                 PHY_PAGE_SIZE_BITS
#define PAGE_TABLE_SIZE                 PHY_PAGE_SIZE
#define PAGE_TABLE_ENT_SIZE             REG_SIZE
#define PAGE_TABLE_ENT_BITS             REG_SIZE_BITS
#define PAGE_TABLE_ENT_PER_TABLE        (PAGE_TABLE_SIZE / PAGE_TABLE_ENT_SIZE)
#define PAGE_TABLE_BITS_PER_LEVEL       (PAGE_TABLE_BITS - PAGE_TABLE_ENT_BITS)

#define L0_BITS                         PAGE_TABLE_BITS_PER_LEVEL
#define L1_BITS                         PAGE_TABLE_BITS_PER_LEVEL
#define L2_BITS                         PAGE_TABLE_BITS_PER_LEVEL
#define UNTRANSLATED_BITS               (1 + PHY_PAGE_SIZE_BITS) /* +1 for having two PFNs per VPN */

#define TRANSLATED_BITS                 (L0_BITS + L1_BITS + L2_BITS)
#define MAX_VIRTUAL_PAGES               (1 << TRANSLATED_BITS)

#define UNTRANSLATED_PAGE_SIZE          (1 << UNTRANSLATED_BITS)

#define VTABLE_ENTRY_USED               (-1)
#define VTABLE_ENTRY_TRAN               (-2)
#define REVOKE_STATE_AVAIL      0
#define REVOKE_STATE_STARTED    1
#define REVOKE_STATE_REVOKING   2
#define REVOKE_STATE_TRANS      3

#define PFN_SHIFT                       6
/* These bits will eventually be untranslated high bits, but we will check they are equal to a field in the leaf
 * Of the page table. These could be considered a generation count. */

#define CHECKED_BITS                    (64 - L0_BITS - L1_BITS - L2_BITS - UNTRANSLATED_BITS)


#define L0_INDEX(addr)          ((addr << CHECKED_BITS) >> (CHECKED_BITS + L1_BITS + L2_BITS + UNTRANSLATED_BITS))
#define L1_INDEX(addr)          ((addr << CHECKED_BITS + L0_BITS) >> (CHECKED_BITS + L0_BITS + L2_BITS + UNTRANSLATED_BITS))
#define L2_INDEX(addr)          ((addr << CHECKED_BITS + L0_BITS + L1_BITS) >> (CHECKED_BITS + L0_BITS + L1_BITS + UNTRANSLATED_BITS))
#define PAGE_INDEX(addr)        (addr & (PHY_PAGE_SIZE - 1))
#define GENERATION_COUNT(addr)  (addr >> (L0_BITS + L1_BITS + L2_BITS + UNTRANSLATED_BITS))

#define PAGE_SIZE                 (PHY_PAGE_SIZE)

// Anything less than transaction can be split/merged.
#define NANO_KERNEL_PAGE_STATUS_ENUM_LIST(ITEM)    \
    ITEM(page_unused, 0)                           \
    ITEM(page_nano_owned, 1)                       \
    ITEM(page_system_owned, 2)                     \
    ITEM(page_mapped, 3)                           \
    ITEM(page_ptable, 4)                           \
    ITEM(page_ptable_free, 5)                      \
    ITEM(page_io, 6)                               \
    ITEM(page_dirty, 7)                            \
    ITEM(page_transaction, 8)                      \
    ITEM(page_cleaning, 9)                         \

#define NANO_KERNEL_RES_STATUS_ENUM_LIST(ITEM) \
    ITEM(res_open,          0)                  \
    ITEM(res_taken,         1)                  \
    ITEM(res_merged,        2)                  \
    ITEM(res_trans,         3)                  \
    ITEM(res_revoking,      4)

DECLARE_ENUM(e_res_status, NANO_KERNEL_RES_STATUS_ENUM_LIST)

DECLARE_ENUM(e_page_status, NANO_KERNEL_PAGE_STATUS_ENUM_LIST)

/* Stuff to do with the foundation system */

#define FOUNDATION_ID_SIZE                      (32 + (8 * 4))      // sha256 + other fields
#define FOUNDATION_ID_LEN_OFFSET                32
#define FOUNDATION_ID_E0_OFFSET                 40
#define FOUNDATION_ID_NENT_OFFSET               48
#define FOUNDATION_META_DATA_OFFSET             FOUNDATION_ID_SIZE
#define FOUNDATION_META_ENTRY_VECTOR_OFFSET     (FOUNDATION_ID_SIZE + CAP_SIZE)
#define FOUNDATION_META_SIZE(N)                 (FOUNDATION_ID_SIZE + CAP_SIZE + (N * CAP_SIZE))

#define RES_CERT_META_SIZE                      (3 * CAP_SIZE)

#ifndef __ASSEMBLY__

#define REG_SIZE        sizeof(register_t)
#define REG_SIZE_BITS   3

_Static_assert((1 << REG_SIZE_BITS) == REG_SIZE, "This should be true");

#define BOOK_END                        ((size_t)(TOTAL_PHY_PAGES))

/* WARN: these structures are used in assembly */

typedef capability context_t;               // Type of a nanokernel context handle
typedef capability res_t;                   // Type of a reservation handle
typedef capability tres_t;
typedef capability ptable_t;                // Type of a page table handle


typedef capability entry_t;                 // Type of a foundation entry handle
typedef capability cert_t;                  // A certificate for capability
typedef capability locked_t;                // A locked capability

/* Identifying information for a foundation */
typedef struct found_id_t {
    char sha256[256/8];
    size_t length;
    size_t e0;
    size_t nentries;
    size_t pad;
} found_id_t;

typedef struct type_res_bitfield_t {
    char bitfield[TRES_BITFIELD_SIZE];
} type_res_bitfield_t;

/* This is how big the structure is in the nano kernel */
_Static_assert(sizeof(found_id_t) == FOUNDATION_ID_SIZE, "Assumed by nano kernel");

typedef capability cert_t;                  // A certified capability
typedef capability locked_t;               // A capability that can be unlocked by intended code

typedef struct {
    e_page_status	status;
    size_t	len; /* number of pages in this chunk */
    size_t	prev; /* start of previous chunk */
    size_t  spare; /* Will probably use this to store a VPN or user data */
} page_t;

typedef register_t table_entry_t;
typedef struct {
    table_entry_t entries[PAGE_TABLE_ENT_PER_TABLE];
} readable_table_t;

_Static_assert(sizeof(table_entry_t) == PAGE_TABLE_ENT_SIZE, "Used by nano kernel");

/* This is how big the structure is in the nano kernel */
_Static_assert(sizeof(page_t) == PHY_PAGE_ENTRY_SIZE, "Assumed by nano kernel");

typedef struct {
    context_t victim_context;
    register_t cause;
    register_t ccause;
    register_t badvaddr;
    register_t ex_level;
} exection_cause_t;

typedef struct {
    register_t cause;
    register_t ccause;
} user_exception_cause_t;

typedef register_t ex_lvl_t;
typedef register_t cause_t;

#define MAGIC_SAFE         "ori    $zero, $zero, 0xd00d            \n"

#define ENUM_VMEM_SAFE_DEREFERENCE(location, result, edefault)  \
    __asm__ (                                                   \
        SANE_ASM                                                \
        "li     %[res], %[def]               \n"                \
        "clw    %[res], $zero, 0(%[state])   \n"                \
        MAGIC_SAFE \
    : [res]"=r"(result)                                         \
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


typedef struct res_nfo_t {
    size_t length;
    size_t base;
} res_nfo_t;

#define MAKE_NFO(l, b) (res_nfo_t){.length = l, .base = b}
#endif // __ASSEMBLY__

#endif //CHERIOS_NANOTYPES_H
