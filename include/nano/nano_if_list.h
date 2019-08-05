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

#ifdef SMP_ENABLED
#define NANO_KERNEL_IF_RAW_LIST_SMP(ITEM, ...)\
        /* Switches an -IDLE- SMP core to context start_as */\
        ITEM(smp_context_start, int, (context_t, start_as, register_t, cpu_id), __VA_ARGS__)
#else
#define NANO_KERNEL_IF_RAW_LIST_SMP(ITEM, ...)
#endif

#define NANO_KERNEL_IF_RAW_LIST(ITEM, ...)                                          \
/* TODO in order to do SGX like things we may have an argument that means "and give them a new capability" */\
/* Creates a context from a intial reg_frame and returns a handle. If the reservation provided is NULL will allocate\
 * in a limited static region */\
    ITEM(create_context, context_t, (reg_frame_t*, initial_state, res_t res), __VA_ARGS__)  \
/* Deletes a context, restore_from is ONLY used if a context is destroying itself */\
    ITEM(destroy_context, context_t, (register_t, a0, register_t, a1, register_t, a2, register_t, a3, register_t, v0, register_t, v1, capability, c3, capability, c4, capability, c5, capability, c6, capability, c1, context_t, restore_from, context_t, destroy), __VA_ARGS__) \
/* Switch to a handle, and store a handle for the suspended context to the location pointed to by store_to */\
    ITEM(context_switch, void, (register_t, a0, register_t, a1, register_t, a2, register_t, a3, register_t, v0, register_t, v1, capability, c3, capability, c4, capability, c5, capability, c6, capability, c1, context_t, restore_from), __VA_ARGS__) \
/* Delays interrupts until exit is called the same number of times OR context_switch is called */\
    ITEM(critical_section_enter, uint8_t, (void), __VA_ARGS__) \
    ITEM(critical_section_exit, void, (void), __VA_ARGS__) \
/* TODO a better interface would consist of two things: A chain of contexts to restore, and a chain per exception type.
 * TODO my thoughts are these: In the event of an interrupt, it seems silly to restore a special context that decides
 * TODO who to switch to. If it was a timer interrupt we can just schedule the next person in a list. If it was a
 * TODO specific asyn interrupt, some activation (which might have a high priority) might be waiting on that. In this
 * TODO case we might want to restore that context straight away */ \
    ITEM(set_exception_handler, void, (context_t, context, register_t ,cpu_id), __VA_ARGS__) \
/* Returns a proper capability made from a reservation. state open -> taken. Fails if not open */\
/* This secretly also returns by value in c3/c4. This is guaranteed, but there is no way to declare that in C*/\
    ITEM(rescap_take, void, (res_t, res, cap_pair*, out), __VA_ARGS__)\
/* Returns the length/base of a reservation (not including any metadata) */\
    ITEM(rescap_nfo, res_nfo_t, (res_t, res), __VA_ARGS__)\
/* Tells the revokeer to start revoking this reservation. revoking fails if already revoking. Must be MAPPED */\
    ITEM(rescap_revoke_start, int, (res_t, res), __VA_ARGS__)\
/* Tells the revokeer to finish revoking this reservation from before. Must be UNMAPPED. */\
    ITEM(rescap_revoke_finish, res_t, (uint64_t*, bytes_scanned), __VA_ARGS__)\
/* Splits an open reservation. The reservation will have size `size'. The remaining space will be returned as a new reservation. */\
    ITEM(rescap_split, res_t, (capability, res, size_t, size), __VA_ARGS__)\
/* Merges two taken reservations. Cannot merge with revoking. If an open and taken are merged the result is taken*/\
    ITEM(rescap_merge, res_t, (res_t, res1, res_t, res2), __VA_ARGS__)\
/* Create a node. Argument must be open, will transition to taken, and a single child will be created and returned*/\
    ITEM(rescap_parent, res_t, (res_t, res), __VA_ARGS__)\
/* Makes this reservation subdividable. Is now considered taken - but you can get subservations with getsub*/\
    ITEM(rescap_splitsub, res_t, (res_t, res, register_t, scale),__VA_ARGS__)\
/* Like split, but the size will be (2 ^ scale) that was set with splitsub. Cannot be split further */\
    ITEM(rescap_getsub, res_t, (res_t, res, register_t, index),__VA_ARGS__)\
/* Get a physical page. Can only be done if the page is not nano owned, or mapped to from a virtual address*/\
/* If IO the pages won't be scanned for revoke - but can't be used to store capabilities */\
    ITEM(get_phy_page, void, (register_t, page_n, int, cached, register_t, npages, cap_pair*, out, register_t, IO), __VA_ARGS__)\
/* Allocate a physical page to be page table. */\
    ITEM(create_table, ptable_t, (register_t, page_n, ptable_t, parent, register_t, index),  __VA_ARGS__)\
/* Map an entry in a leaf page table to a physical page. The adjacent physical page will also be mapped */\
    ITEM(create_mapping, void, (register_t, page_n, ptable_t, table, register_t, index_start, register_t, index_stop, register_t, flags),  __VA_ARGS__) \
/* Free a mapping. This will recover the physical page */\
    ITEM(free_mapping, void, (ptable_t, table, register_t, index), __VA_ARGS__)   \
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
/* The page must have a non zero length. Will make its length new length */\
    ITEM(split_phy_page_range, void, (register_t, pagen, register_t, new_len), __VA_ARGS__)\
/* The page must have non zero length, and the the next page must have identical status. Merges the records. */\
    ITEM(merge_phy_page_range, void, (register_t, pagen), __VA_ARGS__)\
/* The page must have non zero length, will go from state dirty -> free when finished */\
    ITEM(zero_page_range, void, (register_t, pagen), __VA_ARGS__)\
/* FIXME for debug ONLY. When we have proper debugging, this must be removed. It defeats the whole point. */\
/* For debugging. Returns a global cap and gives your pcc all the permission bits it can */\
    ITEM(obtain_super_powers, capability, (void), __VA_ARGS__)\
/* Get the victim context and cause register for the last exception. */\
    ITEM(get_last_exception, void, (exection_cause_t*, out), __VA_ARGS__)\
/* Create a new founded code block. The entry returned will be at offset entry0. */\
/* A public foundation has no associated data, but the entries can be converted into readable capabilities. */\
    ITEM(foundation_create, entry_t,                                                                            \
    (res_t, res, size_t, image_size, capability, image, size_t, entry0, size_t, n_entries, register_t, is_public),   \
    __VA_ARGS__)\
/* Enter a foundation created with foundation_create or foundation_new_entry. The entry will be jumped to, and optionally a locked idc will be unclocked into idc*/\
/* c8 will contain the data componant of the foundation. c9 will have a copy of idc. c12 will be correctly set to target as well.
 * This function acts a whole lot like ccall entry, lock_idc but with foundations instead of sealed objects*/\
    ITEM(foundation_enter, void, (capability, c3, capability, c4, capability, c5, capability, c6, capability, c7, entry_t, entry, invocable_t, lock_idc), __VA_ARGS__)\
/* Unlock a public foundation */\
    ITEM(foundation_entry_expose, capability, (entry_t, entry), __VA_ARGS__)\
/* Gets the -canonical- foundation id for an entry */\
    ITEM(foundation_entry_get_id, found_id_t*, (entry_t, entry), __VA_ARGS__)\
/* Get a cryptograpic key for this auth token. Is HMAC(nano_master_key, found_id).*/\
    ITEM(make_key_for_auth, found_key_t*, (res_t, res, auth_t, auth), __VA_ARGS__)\
/* Get the vaddr of an entry point. Mostly useful for debugging, this is not a capability */\
    ITEM(foundation_entry_vaddr, register_t, (entry_t, entry), __VA_ARGS__)\
/* Create a new entry from within a foundation */\
    ITEM(foundation_new_entry, entry_t, (size_t, eid, capability, at, auth_t, auth), __VA_ARGS__)\
/* Take and sign a reservation. Result depends on type used. If no cap_pair* out is provided, will sign inputs code and data. Otherwise signs the result of taking the reservation minus metadata size and also returns them via out.*/\
    ITEM(rescap_take_authed, auth_result_t, (res_t, res, cap_pair*, out, register_t, user_perms, auth_types_t, type, auth_t, auth, capability, code, capability, data), __VA_ARGS__)\
/* Get access to certfied capability from signed handle. Only has user_perms. Will return identity of signer*/\
    ITEM(rescap_check_cert, found_id_t*, (cert_t, cert, cap_pair*, out), __VA_ARGS__)\
/* Get access to certfied capability from signed handle. Only has user_perms. Will return identity of signer. Destructive of the cert.*/\
    ITEM(rescap_check_single_cert, found_id_t*, (single_use_cert, cert, cap_pair*, out), __VA_ARGS__)\
/* Take a reservation and lock for intended user (identified by id). See rescap_take_authed for how out/code/data works. */\
    ITEM(rescap_take_locked, locked_t, (res_t, res, cap_pair*, out, register_t, user_perms, found_id_t*, recipient_id, capability, code, capability, data), __VA_ARGS__)\
/* Unlock a locked capability. Can only be done inside correct foundation */\
    ITEM(rescap_unlock, void, (auth_result_t, locked, cap_pair*, out, auth_t, auth, auth_types_t, type), __VA_ARGS__)\
/* If in a foundation get own foundation_id */\
    ITEM(foundation_get_id, found_id_t*, (auth_t, auth), __VA_ARGS__)\
    NANO_KERNEL_IF_RAW_LIST_SMP(ITEM,__VA_ARGS__)\
/* Gets a type reservation */\
    ITEM(tres_get, tres_t, (register_t, type), __VA_ARGS__)\
/* Turns in a type reservation for a sealing capability */\
    ITEM(tres_take, sealing_cap, (tres_t, tres), __VA_ARGS__)\
/* Revokes a range of types */\
    ITEM(tres_revoke, capability, (register_t, start, register_t, end), __VA_ARGS__)\
/* Gets a capability to a bit vector representing the state of the type space */\
    ITEM(tres_get_ro_bitfield, type_res_bitfield_t*, (void), __VA_ARGS__)\
/* Subscribe user to handle its own exceptions. */\
    ITEM(exception_subscribe, void, (void), __VA_ARGS__)\
/* Return from an exception (pcc, idc and c1 are allowed to have been changed */\
    ITEM(exception_return, void, (void), __VA_ARGS__)\
/* Same as return. But ALSO replay exception that the user handled to the exception context */\
    ITEM(exception_replay, void, (void), __VA_ARGS__)\
/* Send a user generated signal to another context */\
    ITEM(exception_signal, void, (context_t, other_context, register_t, code), __VA_ARGS__)\
/* Get the last user exception */\
    ITEM(exception_getcause, user_exception_cause_t, (void), __VA_ARGS__)\
/* Get a capability that grants no permissions but can take any offset for sealing integers */\
    ITEM(get_integer_space_cap, capability, (void), __VA_ARGS__)\
/* Gets the value of a hardware register, and also updates the bits set in mask to be those in value TODO: this is temporary*/\
    ITEM(modify_hardware_reg, register_t, (e_reg_select, selector, register_t, mask, register_t, value), __VA_ARGS__)\
/* Mask an interrupt 'n' on 'cpu' according to enable */\
    ITEM(interrupts_mask, void, (uint8_t, cpu, register_t, n, int, enable), __VA_ARGS__)\
/* Set the interrupt state of a software interrupt bit */\
    ITEM(interrupts_soft_set, void, (uint8_t, cpu, register_t, n, int, enable), __VA_ARGS__)\
/* Get a 64 bit value of IPs of interrupts. [0-INTERRUPTS_N_SW) are SOFTWARE. [INTERRUPTS_N_SW, INTERRUPTS_N) are HARDWARE*/\
/* The last HW bit is the timer */\
    ITEM(interrupts_get, uint64_t, (uint8_t, cpu), __VA_ARGS__)\
/* Perform an address translation (sadly not hardware accelerated). Will touch the address if need be unless dont_commit */\
    ITEM(translate_address, uint64_t, (uint64_t, virt_addr, int, dont_commit), __VA_ARGS__)\
/* Remove the right to request certain functions by anding the bitvector with a given mask */\
    ITEM(if_req_and_mask, if_req_auth_t, (if_req_auth_t, req_auth, register_t, mask), __VA_ARGS__)\
    ITEM(nano_dummy, void, (void), __VA_ARGS__)
/* TODO We need a method to convert something certified and encrypt it for remote attestation */

#define RAW_TO_NORMAL(name, ret, raw_sig, X, ...) X(name, ret, MAKE_SIG(raw_sig), __VA_ARGS__)

#define NANO_KERNEL_IF_LIST(ITEM, ...) NANO_KERNEL_IF_RAW_LIST(RAW_TO_NORMAL, ITEM, __VA_ARGS__)

#define N_NANO_CALLS (LIST_LENGTH(NANO_KERNEL_IF_RAW_LIST))
#endif //CHERIOS_NANO_IF_LIST_H
