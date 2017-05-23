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

#include "cheric.h"
#include "cheriplt.h"

typedef capability context_t;
typedef capability res_t;

#define RES_PRIV_SIZE                  (sizeof(register_t) * 4)
#define RES_META_SIZE                  256
#define RES_USER_SIZE                  (RES_META_SIZE - RES_PRIV_SIZE)


#define PAGE_SIZE (0x1000)
#define PAGE_TABLE_ENTS (PAGE_SIZE / 8)
typedef capability ptable_t;

#define PHY_MEM_SIZE (1L << 32)
#define TOTAL_PHY_PAGES (PHY_MEM_SIZE/PAGE_SIZE)
#define BOOK_END ((size_t)(TOTAL_PHY_PAGES))

typedef enum e_page_status {
    page_unused,
    page_nano_owned,
    page_system_owned,
    page_mapped,
} e_page_status;

//TODO make this dynamic

typedef struct {
    e_page_status	status;
    size_t	len; /* number of pages in this chunk */
    size_t	prev; /* start of previous chunk */
    size_t  spare; /* Will probably use this to store a VPN or user data */
} page_t;

/* This is how big the structure is in the nano kernel */
_Static_assert(sizeof(page_t) == 4 * sizeof(register_t), "Assumed by nano kernel");


typedef struct {
    context_t victim_context;
    register_t cause;
    register_t ccause;
} exection_cause_t;

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
    ITEM(rescap_take, capability, (res_t res), __VA_ARGS__)\
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
    ITEM(get_phy_page, capability, (register_t page_n, register_t cached), __VA_ARGS__)\
/* Allocate a physical page to be page table. */\
    ITEM(create_table, ptable_t, (register_t page_n, ptable_t parent, register_t index),  __VA_ARGS__)\
/* Map an entry in a leaf page table to a physical page. The adjacent physical page will also be mapped */\
    ITEM(create_mapping, void, (register_t page_n, ptable_t table, register_t index),  __VA_ARGS__) \
/* Get a handle for the top level page table*/\
    ITEM(get_top_level_table, ptable_t, (void), __VA_ARGS__) \
/* Create the reservation for all of virtual memory */\
    ITEM(make_first_reservation, res_t, (void), __VA_ARGS__) \
/* Get a read only capability to the nano kernels book */\
    ITEM(get_book, page_t*, (void), __VA_ARGS__) \
/* Thw page must have a non zero length. Will make its length new length */\
    ITEM(split_phy_page_range, void, (register_t pagen, register_t new_len), __VA_ARGS__)\
/* FIXME for debug ONLY. When we have proper debugging, this must be removed. It defeats the whole point. */\
/* For debugging. Returns a global cap and gives your pcc all the permission bits it can */\
    ITEM(obtain_super_powers, capability, (void), __VA_ARGS__)\
/* Gets a R/W cap for the userdata space of a meta data node of a reservation. */\
    ITEM(get_userdata_for_res, capability, (res_t res), __VA_ARGS__)\
/* Get the victim context and cause register for the last exception. */\
    ITEM(get_last_exception, void, (exection_cause_t* out), __VA_ARGS__)

PLT(nano_kernel_if_t, NANO_KERNEL_IF_LIST)

#define ALLOCATE_PLT_NANO PLT_ALLOCATE(nano_kernel_if_t, NANO_KERNEL_IF_LIST)

/* Current not able to request multiple pages. Only really for getting access to magic regs w/o virtual memory */
/* Try to ask memgt instead of using this */
static inline capability get_phy_cap(page_t* book, size_t address, size_t size, register_t cached) {
    size_t phy_page = address / PAGE_SIZE;
    size_t phy_offset = address & (PAGE_SIZE - 1);

    if(book[phy_page].len == 0) {
        size_t search_index = 0;
        while(book[search_index].len + search_index < phy_page) {
            search_index = search_index + book[search_index].len;
        }
        split_phy_page_range(search_index, phy_page - search_index);
    }

    split_phy_page_range(phy_page, 1);
    capability cap_for_phy = get_phy_page(phy_page, cached);
    cap_for_phy = cheri_setoffset(cap_for_phy, phy_offset);
    cap_for_phy = cheri_setbounds(cap_for_phy, size);
    return cap_for_phy;
}

#endif //CHERIOS_NANOKERNEL_H
