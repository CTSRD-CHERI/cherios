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

#ifndef CHERIOS_NANO_IF_LIST_H
#define CHERIOS_NANO_IF_LIST_H

#include "macroutils.h"

/* Define the arguments with commas so we can extract just arguments if we want */

#define NANO_KERNEL_IF_RAW_LIST(ITEM, ...)                                          \
/* TODO in order to do SGX like things we may have an argument that means "and give them a new capability" */\
/* Creates a context from a intial reg_frame and returns a handle */\
    ITEM(create_context, context_t, (reg_frame_t*, initial_state), __VA_ARGS__)  \
/* Deletes a context, restore_from is ONLY used if a context is destroying itself */\
    ITEM(destroy_context, context_t, (context_t, context, context_t, restore_from), __VA_ARGS__) \
/* Switch to a handle, and store a handle for the suspended context to the location pointed to by store_to */\
    ITEM(context_switch, void, (context_t, restore_from, context_t*,  store_to), __VA_ARGS__) \
/* Delays interrupts until exit is called the same number of times OR context_switch is called */\
    ITEM(critical_section_enter, void, (void), __VA_ARGS__) \
    ITEM(critical_section_exit, void, (void), __VA_ARGS__) \
/* TODO a better interface would consist of two things: A chain of contexts to restore, and a chain per exception type.
 * TODO my thoughts are these: In the event of an interrupt, it seems silly to restore a special context that decides
 * TODO who to switch to. If it was a timer interrupt we can just schedule the next person in a list. If it was a
 * TODO specific asyn interrupt, some activation (which might have a high priority) might be waiting on that. In this
 * TODO case we might want to restore that context straight away */ \
    ITEM(set_exception_handler, void, (context_t, context), __VA_ARGS__) \
/* Returns a proper capability made from a reservation. state open -> taken. Fails if not open */\
    ITEM(rescap_take, void, (res_t, res, cap_pair*, out), __VA_ARGS__)\
/* Returns a SEALED version of rescap_take, but does not change state. Then get fields using normal ops */\
    ITEM(rescap_info, capability, (res_t, res), __VA_ARGS__)\
/* Tells the revokeer to start revoking this reservation. revoking fails if already revoking */\
    ITEM(rescap_revoke, res_t, (res_t, res), __VA_ARGS__)\
/* Splits an open reservation. The reservation will have size `size'. The remaining space will be returned as a new reservation. */\
    ITEM(rescap_split, res_t, (capability, res, size_t, size), __VA_ARGS__)\
/* Merges two taken reservations. Cannot merge with revoking. If an open and taken are merged the result is taken*/\
    ITEM(rescap_merge, res_t, (res_t, res1, res_t, res2), __VA_ARGS__)\
/* Create a node. Argument must be open, will transition to taken, and a single child will be created and returned*/\
    ITEM(rescap_parent, res_t, (res_t, res), __VA_ARGS__)\
/* Get a physical page. Can only be done if the page is not nano owned, or mapped to from a virtual address*/\
    ITEM(get_phy_page, void, (register_t, page_n, int, cached, register_t, npages, cap_pair*, out), __VA_ARGS__)\
/* Allocate a physical page to be page table. */\
    ITEM(create_table, ptable_t, (register_t, page_n, ptable_t, parent, register_t, index),  __VA_ARGS__)\
/* Map an entry in a leaf page table to a physical page. The adjacent physical page will also be mapped */\
    ITEM(create_mapping, void, (register_t, page_n, ptable_t, table, register_t, index, register_t, flags),  __VA_ARGS__) \
/* Free a mapping. This will recover the physical page */\
    ITEM(free_mapping, void, (ptable_t, table, register_t, index), __VA_ARGS__)   \
/* Use a reservation to show a virtual range can be remappep */\
    ITEM(clear_mapping, void, (ptable_t, table, register_t, vaddr, res_t, reservation), __VA_ARGS__)\
/* Get a handle for the top level page table*/\
    ITEM(get_top_level_table, ptable_t, (void), __VA_ARGS__) \
/* Get a handle for a sub table from an L0 or L1 table. */\
    ITEM(get_sub_table, ptable_t, (ptable_t, table, register_t, index), __VA_ARGS__)\
/* Get a Read-Only cap for the table */\
    ITEM(get_read_only_table, readable_table_t*, (ptable_t, table), __VA_ARGS__)\
/* Create the reservation for all of virtual memory */\
    ITEM(make_first_reservation, res_t, (void), __VA_ARGS__) \
/* Get a read only capability to the nano kernels book */\
    ITEM(get_book, page_t*, (void), __VA_ARGS__) \
/* Thw page must have a non zero length. Will make its length new length */\
    ITEM(split_phy_page_range, void, (register_t, pagen, register_t, new_len), __VA_ARGS__)\
/* The page must have non zero length, and the the next page must have identical status. Merges the records. */\
    ITEM(merge_phy_page_range, void, (register_t, pagen), __VA_ARGS__)\
/* FIXME for debug ONLY. When we have proper debugging, this must be removed. It defeats the whole point. */\
/* For debugging. Returns a global cap and gives your pcc all the permission bits it can */\
    ITEM(obtain_super_powers, capability, (void), __VA_ARGS__)\
/* Gets a R/W cap for the userdata space of a meta data node of a reservation. */\
    ITEM(get_userdata_for_res, capability, (res_t, res), __VA_ARGS__)\
/* Get the victim context and cause register for the last exception. */\
    ITEM(get_last_exception, void, (exection_cause_t*, out), __VA_ARGS__)\
/* For both diagnostic and performance reasons a r.w capability to the EL is available. Use this for a fast critical region*/\
    ITEM(get_critical_level_ptr, ex_lvl_t*,  (void), __VA_ARGS__)\
    ITEM(get_critical_cause_ptr, cause_t*,  (void), __VA_ARGS__)




#define RAW_TO_NORMAL(name, ret, raw_sig, X, ...) X(name, ret, MAKE_SIG(raw_sig), __VA_ARGS__)

#define NANO_KERNEL_IF_LIST(ITEM, ...) NANO_KERNEL_IF_RAW_LIST(RAW_TO_NORMAL, ITEM, __VA_ARGS__)
#endif //CHERIOS_NANO_IF_LIST_H
