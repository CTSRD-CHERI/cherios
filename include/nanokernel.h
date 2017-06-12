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

#ifndef CHERIOS_NANOKERNEL_H
#define CHERIOS_NANOKERNEL_H

#include "string_enums.h"

#ifndef __ASSEMBLY__

#include "cheric.h"
#include "cheriplt.h"
#include "mman.h"

#define REG_SIZE        sizeof(register_t)
#define REG_SIZE_BITS   3

_Static_assert((1 << REG_SIZE_BITS) == REG_SIZE, "This should be true");

#else // __ASSEMBLY__

#endif

// FIXME we need to choose appropriate types and remove their accessibility from the rest of the system

/* The types used for nano kernel objects. */
#define CONTEXT_TYPE       0x5555         // The type of contexts
#define NANO_KERNEL_TYPE   0x6666         // The type of sealed local data
#define RES_TYPE           0x7777         // The type of a reservation handle
#define RES_VIEW_TYPE      0x7778         // The type of a sealed capability covering the range of a reservation
#define VTABLE_TYPE_L0     0x8880         // The type of the top level page table
#define VTABLE_TYPE_L1     VTABLE_TYPE_L0 + 1  // The type of the L1 level page table
#define VTABLE_TYPE_L2     VTABLE_TYPE_L0 + 2  // The type of the L2 level page table

#define VTABLE_LEVELS      3              // Number of table levels


/* Size of metadata for reservations. Split into private and user data */
#define RES_PRIV_SIZE                   (REG_SIZE * 4)
#define RES_META_SIZE                   (256)
#define RES_USER_SIZE                   (RES_META_SIZE - RES_PRIV_SIZE)

/* Page sizes etc */
#define PHY_MEM_SIZE                    (1L << 32)


/* TODO: Would like to increase this but QEMU does not seem to support a page pask not 0 */
#define PHY_PAGE_SIZE_BITS              (12)
#define PHY_PAGE_SIZE                   (1 << PHY_PAGE_SIZE_BITS)
#define TOTAL_PHY_PAGES                 (PHY_MEM_SIZE/PAGE_SIZE)

#define PHY_ADDR_TO_PAGEN(addr)         ((addr >> PHY_PAGE_SIZE_BITS) & (TOTAL_PHY_PAGES-1))


/* Physical page records */
#define PHY_PAGE_ENTRY_SIZE_BITS        (REG_SIZE_BITS + 2)
#define PHY_PAGE_ENTRY_SIZE             (1L << PHY_PAGE_ENTRY_SIZE_BITS)

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

#define VTABLE_ENTRY_USED               (-1)

#define PFN_SHIFT                       6
/* These bits will eventually be untranslated high bits, but we will check they are equal to a field in the leaf
 * Of the page table. These could be considered a generation count. */

#define CHECKED_BITS                    (63 - L0_BITS - L1_BITS - L2_BITS - UNTRANSLATED_BITS)


#define L0_INDEX(addr)          ((addr << CHECKED_BITS) >> (CHECKED_BITS + L1_BITS + L2_BITS + UNTRANSLATED_BITS))
#define L1_INDEX(addr)          ((addr << CHECKED_BITS + L0_BITS) >> (CHECKED_BITS + L0_BITS + L2_BITS + UNTRANSLATED_BITS))
#define L2_INDEX(addr)          ((addr << CHECKED_BITS + L0_BITS + L1_BITS) >> (CHECKED_BITS + L0_BITS + L1_BITS + UNTRANSLATED_BITS))
#define PAGE_INDEX(addr)        (addr & (PHY_PAGE_SIZE - 1))
#define GENERATION_COUNT(addr)  (addr >> (L0_BITS + L1_BITS + L2_BITS + UNTRANSLATED_BITS))

#define PAGE_SIZE                 (PHY_PAGE_SIZE)

#define NANO_KERNEL_IF_LIST(ITEM, ...)                                          \
/* TODO in order to do SGX like things we may have an argument that means "and give them a new capability" */\
/* Creates a context from a intial reg_frame and returns a handle */\
    ITEM(create_context, context_t, (reg_frame_t* initial_state), __VA_ARGS__)  \
/* Deletes a context, restore_from is ONLY used if a context is destroying itself */\
    ITEM(destroy_context, context_t, (context_t context, context_t restore_from), __VA_ARGS__) \
/* Switch to a handle, and store a handle for the suspended context to the location pointed to by store_to */\
    ITEM(context_switch, void, (context_t restore_from, context_t*  store_to), __VA_ARGS__) \
/* Delays interrupts until exit is called the same number of times OR context_switch is called */\
    ITEM(critical_section_enter, void, (void), __VA_ARGS__) \
    ITEM(critical_section_exit, void, (void), __VA_ARGS__) \
/* TODO a better interface would consist of two things: A chain of contexts to restore, and a chain per exception type.
 * TODO my thoughts are these: In the event of an interrupt, it seems silly to restore a special context that decides
 * TODO who to switch to. If it was a timer interrupt we can just schedule the next person in a list. If it was a
 * TODO specific asyn interrupt, some activation (which might have a high priority) might be waiting on that. In this
 * TODO case we might want to restore that context straight away */ \
    ITEM(set_exception_handler, void, (context_t context), __VA_ARGS__) \
/* Returns a proper capability made from a reservation. state open -> taken. Fails if not open */\
    ITEM(rescap_take, void, (res_t res, cap_pair* out), __VA_ARGS__)\
/* Returns a SEALED version of rescap_take, but does not change state. Then get fields using normal ops */\
    ITEM(rescap_info, capability, (res_t res), __VA_ARGS__)\
/* Tells the collector to start collecting this reservation. collecting fails if already collecting */\
    ITEM(rescap_collect, res_t, (res_t res), __VA_ARGS__)\
/* Splits an open reservation. The reservation will have size `size'.\
 * The remaining space will be returned as a new reservation. */\
    ITEM(rescap_split, res_t, (capability res, size_t size), __VA_ARGS__)\
/* Merges two taken reservations. Cannot merge with collecting. If an open and taken are merged the result is taken*/\
    ITEM(rescap_merge, res_t, (res_t res1, res_t res2), __VA_ARGS__)\
/* Create a node. Argument must be open, will transition to taken, and a single child will be created and returned*/\
    ITEM(rescap_parent, res_t, (res_t res), __VA_ARGS__)\
/* Get a physical page. Can only be done if the page is not nano owned, or mapped to from a virtual address*/\
    ITEM(get_phy_page, void, (register_t page_n, int cached, register_t npages, cap_pair* out), __VA_ARGS__)\
/* Allocate a physical page to be page table. */\
    ITEM(create_table, ptable_t, (register_t page_n, ptable_t parent, register_t index),  __VA_ARGS__)\
/* Map an entry in a leaf page table to a physical page. The adjacent physical page will also be mapped */\
    ITEM(create_mapping, void, (register_t page_n, ptable_t table, register_t index, register_t flags),  __VA_ARGS__) \
/* Free a mapping. This will recover the physical page */\
    ITEM(free_mapping, void, (ptable_t table, register_t index), __VA_ARGS__)   \
/* Use a reservation to show a virtual range can be remappep */\
    ITEM(clear_mapping, void, (ptable_t table, register_t vaddr, res_t reservation), __VA_ARGS__)\
/* Get a handle for the top level page table*/\
    ITEM(get_top_level_table, ptable_t, (void), __VA_ARGS__) \
/* Get a handle for a sub table from an L0 or L1 table. */\
    ITEM(get_sub_table, ptable_t, (ptable_t table, register_t index), __VA_ARGS__)\
/* Get a Read-Only cap for the table */\
    ITEM(get_read_only_table, readable_table_t*, (ptable_t table), __VA_ARGS__)\
/* Create the reservation for all of virtual memory */\
    ITEM(make_first_reservation, res_t, (void), __VA_ARGS__) \
/* Get a read only capability to the nano kernels book */\
    ITEM(get_book, page_t*, (void), __VA_ARGS__) \
/* Thw page must have a non zero length. Will make its length new length */\
    ITEM(split_phy_page_range, void, (register_t pagen, register_t new_len), __VA_ARGS__)\
/* The page must have non zero length, and the the next page must have identical status. Merges the records. */\
    ITEM(merge_phy_page_range, void, (register_t pagen), __VA_ARGS__)\
/* FIXME for debug ONLY. When we have proper debugging, this must be removed. It defeats the whole point. */\
/* For debugging. Returns a global cap and gives your pcc all the permission bits it can */\
    ITEM(obtain_super_powers, capability, (void), __VA_ARGS__)\
/* Gets a R/W cap for the userdata space of a meta data node of a reservation. */\
    ITEM(get_userdata_for_res, capability, (res_t res), __VA_ARGS__)\
/* Get the victim context and cause register for the last exception. */\
    ITEM(get_last_exception, void, (exection_cause_t* out), __VA_ARGS__)\
/* For both diagnostic and performance reasons a r.w capability to the EL is available. Use this for a fast critical region*/\
    ITEM(get_critical_level_ptr, ex_lvl_t*,  (void), __VA_ARGS__)\
    ITEM(get_critical_cause_ptr, cause_t*,  (void), __VA_ARGS__)


#define NANO_KERNEL_PAGE_STATUS_ENUM_LIST(ITEM)    \
    ITEM(page_unused, 0)                           \
    ITEM(page_nano_owned, 1)                       \
    ITEM(page_system_owned, 2)                     \
    ITEM(page_mapped, 3)                           \
    ITEM(page_ptable, 4)                           \
    ITEM(page_ptable_free, 5)                      \

DECLARE_ENUM(e_page_status, NANO_KERNEL_PAGE_STATUS_ENUM_LIST)

#ifndef __ASSEMBLY__

#define BOOK_END                        ((size_t)(TOTAL_PHY_PAGES))

typedef capability context_t;
typedef capability res_t;

typedef capability ptable_t;

//TODO make this dynamic

/* WARN: these structures are used in assembly */

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
} exection_cause_t;

typedef register_t ex_lvl_t;
typedef register_t cause_t;


PLT(nano_kernel_if_t, NANO_KERNEL_IF_LIST)

#define ALLOCATE_PLT_NANO PLT_ALLOCATE(nano_kernel_if_t, NANO_KERNEL_IF_LIST)

/* Try to ask memgt instead of using this */
static inline capability get_phy_cap(page_t* book, size_t address, size_t size, int cached) {
    size_t phy_page = address / PAGE_SIZE;
    size_t phy_offset = address & (PAGE_SIZE - 1);

    if((phy_offset + size) > PAGE_SIZE) {
        /* If you want a better version use mmap */
        return NULL;
    }

    if(book[phy_page].len == 0) {
        size_t search_index = 0;
        while(book[search_index].len + search_index < phy_page) {
            search_index = search_index + book[search_index].len;
        }
        split_phy_page_range(search_index, phy_page - search_index);
    }

    if(book[phy_page].len != 1) {
        split_phy_page_range(phy_page, 1);
    }

    cap_pair pair;
    get_phy_page(phy_page, cached, 1, &pair);
    capability cap_for_phy = pair.data;
    cap_for_phy = cheri_setoffset(cap_for_phy, phy_offset);
    cap_for_phy = cheri_setbounds(cap_for_phy, size);
    return cap_for_phy;
}

#else

#define LOCAL_CAP_VAR_MACRO(item,...)   local_cap_var item ## _cap;
#define INIT_TABLE_MACRO(item,...)      init_table item;

#endif // __ASSEMBLY__

#endif //CHERIOS_NANOKERNEL_H
