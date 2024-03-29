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

#ifndef __ASSEMBLY__
typedef capability context_t;               // Type of a nanokernel context handle
#endif

#include "string_enums.h"
#include "nano/nano_reg_list.h"

#define TYPE_SPACE_BITS     16
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
#define IF_AUTH_TYPE       0x0005         // The type of the capability to request nano kernel capabilities by index using syscall
#define FOUND_AUTH_TYPE    0x0010         // The type of an auth handle (like a private key)
#define FOUND_ENTRY_TYPE   0x0011         // The type of a foundation entry handle
#define FOUND_LOCKED_TYPE  0x0012         // The type of a foundation locked message handle (only unlocked by an auth, made by any)
#define FOUND_CERT_TYPE    0x0013         // The type of a foundation certificate handle (made by an auth, unlocked by any)
#define FOUND_SINGLE_CERT  0x0014         // A certificate that destructs after checking it once
#define FOUND_SYM_TYPE     0x0015         // The type of a foundation symetric locked handle (made by an auth, only unlockable by auth)
#define FOUND_INV_TYPE     0x0016         // The type of a foundation invocation handle (made by an auth, unlockable by auth with found_enter)
#define VTABLE_TYPE_L0     0x0020         // The type of the top level page table
#define VTABLE_TYPE_L1     VTABLE_TYPE_L0 + 1  // The type of the L1 level page table
#define VTABLE_TYPE_L2     VTABLE_TYPE_L0 + 2  // The type of the L2 level page table

#define VTABLE_LEVELS      3              // Number of table levels


/* Size of metadata for reservations. Split into private and user data */
#define RES_PRIV_SIZE                   (0)
#define RES_META_SIZE                   (16)
#define RES_USER_SIZE                   (RES_META_SIZE - RES_PRIV_SIZE)

/* New layout. Length and fields before it can be loaded as 64 bit.
   On big-endian, the other fields were loaded as 16 bits as well.
   The state/term/scale fields are therefore always logically 16 bits.
   Currently I am not loading those 16-bits on their own on little-endian,
   but the idea is that a smaller atomic could be used to change just state and nothing else.
   High and Low terminators mark the first and last children of a particular parent.
   Low terminator technically redundant, and if another bit is needed, should be scavenged.
 */

// | State  | Low Term | High Term | Scale  | Length  | Bitmap  |
// | 2 bits | 1 bits   | 1 bits    | 7 bits | 53 bits | 64 bits |

// if Scale == RES_SCALE_NOT_FIELD, this is not a field. Otherwise it is and scale is its scale.

// Scale is |Exp|Mantissa|. length has RES_LENGTH_SHIFT_LOW zero bits implied at the bottom, giving a 57 bits of length
//          | 5 |   2    |

#define RES_LENGTH_OFFSET               0
// How many high bits in length are other fields
#define RES_LENGTH_SHIFT_HIGH           11
// How many implied 0 bits are at the bottom of length
#define RES_LENGTH_SHIFT_LOW            4

#define RES_STATE_OFFSET                0           // Keep zero for link/conditional

// 16-bit descriptions
#define RES_STATE_MASK                  0xC000
#define RES_LOW_TERM_MASK               0x2000
#define RES_HIGH_TERM_MASK              0x1000
#define RES_SCALE_MASK                  0x0FE0
#define RES_ALL_MASK                    0xFFE0
#define RES_OTHER_MASK                  0x001F

#define RES_STATE_SHIFT                 14
#define RES_SCALE_SHIFT                 5
// These are only for when we load the state as a half
#define RES_SCALE_SHIFT                 5
#define RES_STATE_SHIFT                 14

// The shift to get the 16-bit field
#define RES_STATE_IN_LENGTH_SHIFT       (6 * 8)
#define RES_SIMPLE_SHIFT                12
#define RES_SCALE_NOT_FIELD              0 // A parent or taken, not a field
// Makes the largest field x 64 be 4GB, so that the result of the multiplication will be 32 bit
#define RES_SCALE_MAX                   (0b1100100 - 1)
#define RES_SUBFIELD_BITMAP_OFFSET      8
#define RES_SUBFIELD_BITMAP_BITS        (8 * 8)

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
//   capability __unused__
//   exception_pcc       // PCC that was the exception pcc
//   exception_idc       // IDC that was the exception idc
//   exception_saved_idc // IDC to do swapping with
//};

#define CONTEXT_SIZE                    (CHERI_FRAME_SIZE + (CAP_SIZE * 6))
#define CONTEXT_OFFSET_STATE            (CHERI_FRAME_SIZE - INC_IM_MAX)

// Keep created as zero, or have to rewrite assembly
#define CONTEXT_STATE_CREATED           0
// This state is not used by MIPS, which seems horribly bugged on multicore
#define CONTEXT_STATE_RUNNING           1
// Destroyed should not share any bits with other states and be the largets value (so amo and/max can be used)
#define CONTEXT_STATE_DESTROYED         2

#define CONTEXT_OFFSET_EX_STATE         (CHERI_FRAME_SIZE + REG_SIZE - INC_IM_MAX)

#define EX_STATE_UH                     1
#define EX_STATE_EL                     2

#define CONTEXT_OFFSET_CAUSE            (CHERI_FRAME_SIZE + (2 * REG_SIZE) - INC_IM_MAX)
#define CONTEXT_OFFSET_CCAUSE           (CHERI_FRAME_SIZE + (3 * REG_SIZE) - INC_IM_MAX)

#define CONTEXT_OFFSET_MAGIC           (CHERI_FRAME_SIZE + (2 * CAP_SIZE) - INC_IM_MAX)
#define CONTEXT_OFFSET_EX_PCC           (CHERI_FRAME_SIZE + (3 * CAP_SIZE) - INC_IM_MAX)
#define CONTEXT_OFFSET_EX_IDC           (CHERI_FRAME_SIZE + (4 * CAP_SIZE) - INC_IM_MAX)
#define CONTEXT_OFFSET_EX_SAVED_IDC     (CHERI_FRAME_SIZE + (5 * CAP_SIZE) - INC_IM_MAX)

// These are never re-used are are here for before reservations are working.
#define N_CONTEXTS                      (16)

#define PAGE_START_STATE                page_dirty

/* Physical page records */
#define PHY_PAGE_ENTRY_SIZE_BITS        (REG_SIZE_BITS + 2)
#define PHY_PAGE_ENTRY_SIZE             (1L << PHY_PAGE_ENTRY_SIZE_BITS)
#define PHY_PAGE_OFFSET_status          0
#define PHY_PAGE_OFFSET_len             REG_SIZE
#define PHY_PAGE_OFFSET_prev            (2*REG_SIZE)
#define PHY_PAGE_OFFSET_spare           (3*REG_SIZE)

/* Virtual page table records */

/* In this version we are using one physical page for each page table at each level */

#include "nano/nanotypes_platform.h"

#define PHY_PAGE_SIZE                   (1 << PHY_PAGE_SIZE_BITS)
#define PHY_ADDR_TO_PAGEN(addr)         ((addr >> PHY_PAGE_SIZE_BITS) & 0xFFFFFFFF)

#define PAGE_TABLE_BITS                 PHY_PAGE_SIZE_BITS
#define PAGE_TABLE_SIZE                 PHY_PAGE_SIZE
#define PAGE_TABLE_ENT_SIZE             REG_SIZE
#define PAGE_TABLE_ENT_BITS             REG_SIZE_BITS
#define PAGE_TABLE_ENT_PER_TABLE        (PAGE_TABLE_SIZE / PAGE_TABLE_ENT_SIZE)
#define PAGE_TABLE_BITS_PER_LEVEL       (PAGE_TABLE_BITS - PAGE_TABLE_ENT_BITS)

#define L0_BITS                         PAGE_TABLE_BITS_PER_LEVEL
#define L1_BITS                         PAGE_TABLE_BITS_PER_LEVEL
#define L2_BITS                         PAGE_TABLE_BITS_PER_LEVEL


#define TRANSLATED_BITS                 (L0_BITS + L1_BITS + L2_BITS)
#define MAX_VIRTUAL_PAGES               (1 << TRANSLATED_BITS)

#define UNTRANSLATED_PAGE_SIZE          (1 << UNTRANSLATED_BITS)
#define VIRT_PHY_PAGE_RATIO_BITS        (UNTRANSLATED_BITS - PHY_PAGE_SIZE_BITS)
#define VIRT_PHY_PAGE_RATIO             (1 << VIRT_PHY_PAGE_RATIO_BITS)
#ifdef __ASSEMBLY__
#define T_E_CAST
#else
#define T_E_CAST (table_entry_t)
#endif

#define REVOKE_STATE_AVAIL      0 // can call start
#define REVOKE_STATE_STARTING   1 // in the middle of start
#define REVOKE_STATE_STARTED    2 // can call finish
#define REVOKE_STATE_REVOKING   3 // in the middle of finish

#define PFN_SHIFT                       6
/* These bits will eventually be untranslated high bits, but we will check they are equal to a field in the leaf
 * Of the page table. These could be considered a generation count. */

#define CHECKED_BITS                    (64 - L0_BITS - L1_BITS - L2_BITS - UNTRANSLATED_BITS)


#define L0_INDEX(addr)          (((addr) << CHECKED_BITS) >> (CHECKED_BITS + L1_BITS + L2_BITS + UNTRANSLATED_BITS))
#define L1_INDEX(addr)          (((addr) << (CHECKED_BITS + L0_BITS)) >> (CHECKED_BITS + L0_BITS + L2_BITS + UNTRANSLATED_BITS))
#define L2_INDEX(addr)          (((addr) << (CHECKED_BITS + L0_BITS + L1_BITS)) >> (CHECKED_BITS + L0_BITS + L1_BITS + UNTRANSLATED_BITS))
#define PAGE_INDEX(addr)        ((addr) & (PHY_PAGE_SIZE - 1))
#define GENERATION_COUNT(addr)  ((addr) >> (L0_BITS + L1_BITS + L2_BITS + UNTRANSLATED_BITS))

#define PAGE_SIZE                 (PHY_PAGE_SIZE)

// Anything less than transaction can be split/merged.
// page_cleaning seems like it could just be page_transaction. RISCV does this now, MIPS does not.
// page_unused should always be 0.
#define NANO_KERNEL_PAGE_STATUS_ENUM_LIST(ITEM)    \
    ITEM(page_unused, 0)                           \
    ITEM(page_nano_owned, 1)                       \
    ITEM(page_system_owned, 2)                     \
    ITEM(page_mapped, 3)                           \
    ITEM(page_ptable, 4)                           \
    ITEM(page_ptable_free, 5)                      \
    ITEM(page_io_unused, 6)                        \
    ITEM(page_io_system, 7)                        \
    ITEM(page_io_mapped, 8)                        \
    ITEM(page_dirty, 9)                            \
    ITEM(page_transaction, 10)                     \
    ITEM(page_cleaning, 11)                        \
    ITEM(page_screwed_the_pooch, 12)               \


// Some code assumes open is 0. Others can be re-arranged
// merged is really merged or revoking
// taken is really taken, parented, or split into a subfield

#define NANO_KERNEL_RES_STATUS_ENUM_LIST(ITEM) \
    ITEM(res_open,          0)                  \
    ITEM(res_taken,         1)                  \
    ITEM(res_merged,        2)                  \
    ITEM(res_open_io,       3)

DECLARE_ENUM(e_res_status, NANO_KERNEL_RES_STATUS_ENUM_LIST)

DECLARE_ENUM(e_page_status, NANO_KERNEL_PAGE_STATUS_ENUM_LIST)

DECLARE_ENUM(e_reg_select, NANO_REG_LIST_FOR_ENUM)

/* Stuff to do with the foundation system */

#define FOUNDATION_ID_SIZE                      (32 + (8 * 4))      // sha256 + other fields
#define FOUNDATION_ID_LEN_OFFSET                32
#define FOUNDATION_ID_E0_OFFSET                 40
#define FOUNDATION_ID_NENT_OFFSET               48
#define FOUNDATION_META_DATA_OFFSET             FOUNDATION_ID_SIZE
#define FOUNDATION_META_ENTRY_VECTOR_OFFSET     (FOUNDATION_ID_SIZE + CAP_SIZE)


#define FOUNDATION_META_SIZE(N,L)                 (FOUNDATION_ID_SIZE + CAP_SIZE + (N * CAP_SIZE) + round_cheri_length(L).mask)

#define RES_CERT_META_SIZE                      (3 * CAP_SIZE)

#define RES_CERT_OWNER_OFF 0
#define RES_CERT_CODE_OFF CAP_SIZE
#define RES_CERT_DATA_OFF (2*CAP_SIZE)

#ifdef HARDWARE_qemu
// In queue we will use the cause mips directly and propagate all HW bits, and no software bits.
    #define INTERRUPTS_N_HW     5
    #define INTERRUPTS_N_SW     0
    #define INTERRUPTS_NANO_OWNED   0b11

// Timer is kept seperate and can be accessed via status/cause

#else
// On beri we will cascade all of the systems interrupts to 3:
    #define STATUS_BIT_TO_BERI_IRQ(X) (X - 2)
    #define BERI_IRQ_TO_STATUS_BIT(X) (X + 2)

    #define INTERRUPTS_SYSTEM_N 3

    // we will reformat like this:
    #define INTERRUPTS_N_HW     32
    #define INTERRUPTS_N_SW     32

    // And cascade nano onto 2
    #define INTERRUPTS_NANO_N    2

    #define INTERRUPTS_NANO_OWNED   0b1110100
    #define INTERRUPTS_NANO_PIC_SHOOTDOWN_N   96
    #define INTERRUPTS_NANO_PIC_SHOOTDOWN_OFF (INTERRUPTS_NANO_PIC_SHOOTDOWN_N/8)
    #define INTERRUPTS_NANO_PIC_SHOOTDOWN_BIT (1 << (INTERRUPTS_NANO_PIC_SHOOTDOWN_N & 0b111))

#endif
#define INTERRUPTS_N (INTERRUPTS_N_HW + INTERRUPTS_N_SW)

#ifndef __ASSEMBLY__

_Static_assert(RES_USER_SIZE >= CAP_SIZE, "I shrunk reservation metadata nodes. This probably broke 256.");
_Static_assert(sizeof(register_t) == REG_SIZE, "This should be true");

#define BOOK_END                        ((size_t)(TOTAL_PHY_PAGES))

/* WARN: these structures are used in assembly */

typedef capability res_t;                   // Type of a reservation handle
typedef capability tres_t;
typedef capability ptable_t;                // Type of a page table handle


typedef capability entry_t;                 // Type of a foundation entry handle
typedef capability cert_t;                  // A certificate for capability
typedef capability locked_t;                // A locked capability
typedef capability auth_t;                  // A authorisation to sign and unlock. Granted by found_enter.

typedef capability if_req_auth_t;

/* Identifying information for a foundation. ALSO the public of a pair. Private half is an auth_t */
typedef struct found_id_t {
    uint8_t sha256[256/8];
    size_t length;
    size_t e0;
    size_t nentries;
    size_t pad;
} found_id_t;

typedef struct found_key_t {
    char bytes[256/8];
} found_key_t;

#define FOUND_KEY_SIZE (256/8)

typedef struct type_res_bitfield_t {
    char bitfield[TRES_BITFIELD_SIZE];
} type_res_bitfield_t;

/* This is how big the structure is in the nano kernel */
_Static_assert(sizeof(found_id_t) == FOUNDATION_ID_SIZE, "Assumed by nano kernel");
_Static_assert(sizeof(found_key_t) == FOUND_KEY_SIZE, "Assumed by nano kernel");

typedef capability cert_t;                  // A certified capability
typedef capability single_use_cert;
typedef capability locked_t;               // A capability that can be unlocked by intended code
typedef capability invocable_t;
typedef capability sym_locked_t;

typedef union auth_result_u {
    locked_t locked;
    cert_t cert;
    single_use_cert scert;
    sym_locked_t symetric;
    invocable_t invocable;
} auth_result_t;

typedef enum auth_types_e {
    AUTH_PUBLIC_LOCKED = FOUND_LOCKED_TYPE,
    AUTH_CERT = FOUND_CERT_TYPE,
    AUTH_SINGLE_USE_CERT = FOUND_SINGLE_CERT,
    AUTH_SYMETRIC = FOUND_SYM_TYPE,
    AUTH_INVOCABLE = FOUND_INV_TYPE,
    AUTH_INVALID
} auth_types_t;

static inline int is_of_authed_type(capability c, auth_types_t type) {
    return cheri_gettype(c) == type;
}

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
    register_t cause;
    register_t ccause;
} user_exception_cause_t;

typedef register_t ex_lvl_t;
typedef register_t cause_t;

typedef struct res_nfo_t {
    size_t length;
    size_t base;
} res_nfo_t;

#define MAKE_NFO(l, b) (res_nfo_t){.length = l, .base = b}
#endif // __ASSEMBLY__

#endif //CHERIOS_NANOTYPES_H
