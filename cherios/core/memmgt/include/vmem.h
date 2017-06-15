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

#ifndef CHERIOS_VMEM_H
#define CHERIOS_VMEM_H

#include "sys/mman.h"
#include "nano/nanokernel.h"
#include "assert.h"

#define TLB_ENTRY_CACHE_ALGORITHM_UNCACHED              (2 << 3)
#define TLB_ENTRY_CACHE_ALGORITHM_CACHED_NONCOHERENT    (3 << 3)
#define TLB_ENTRY_VALID                                 2
#define TLB_ENTRY_DIRTY                                 4
#define TLB_ENTRY_GLOBAL                                1

#define TLB_FLAGS_DEFAULT                               (TLB_ENTRY_CACHE_ALGORITHM_CACHED_NONCOHERENT |\
                                                        TLB_ENTRY_VALID | TLB_ENTRY_DIRTY | TLB_ENTRY_GLOBAL)
typedef struct free_chain_t {
    struct used {
        res_t res;
        struct free_chain_t* next_res;
        struct free_chain_t* prev_res;
        struct free_chain_t* next_free_res;
        struct free_chain_t* prev_free_res;
        act_kt allocated_to;
    } used;
    _Static_assert(RES_USER_SIZE >= sizeof(struct used), "We need space for our metadata");
    char spare[(RES_USER_SIZE - sizeof(struct used))];
} free_chain_t;

extern page_t* book;
extern free_chain_t* chain_start;
extern free_chain_t* free_chain_start;

/* Sets page_n to cover a range of len (MUST ALREADY BE A VALID RECORD)*/
void break_page_to(size_t page_n, size_t len);

/* Gets the pagen that can be used to index the book. If in doubt, call this.*/
size_t get_valid_page_entry(size_t page_n);

/* Searches for a range of pages of a particular size and minimum length */
size_t find_page_type(size_t required_len, e_page_status required_type);

/* Get one free page */
size_t get_free_page();

/* Allocates a page table, but finds a physical page for you */
ptable_t memmgt_create_table(ptable_t parent, register_t index);

/* Creates a virt->phy mapping but chooses a physical page for you */
int memmgt_create_mapping(ptable_t L2_table, register_t index, register_t flags);

/* Will free n pages staring at vaddr_start, also freeing tables as required */
void memmgt_free_range(size_t vaddr_start, size_t pages);

/* Takes a reservation from the chain. */
void memgt_take_reservation(free_chain_t* chain, act_kt assign_to, cap_pair* out);
/* creates a parent node in the reservation chain */
res_t memmgt_parent_reservation(free_chain_t* chain, act_kt assign_to);

/* Finds a free reservation. It will include the addr with_addr unless with_addr is 0. Returns NULL if none can be found */
free_chain_t* memmgt_find_free_reservation(size_t with_addr, size_t req_length, size_t* out_base, size_t *out_length);
/* Takes out a reservation and updates metadata */
free_chain_t* memmgt_split_free_reservation(free_chain_t* chain, size_t length);

/* Frees the reservation, tries to merge with adjacent reservations, and frees any vpages / vtables as needed */
free_chain_t* memmgt_free_res(free_chain_t* chain);

/* Given a address that mmap gave out will recover the reservation it came from */
free_chain_t* memmgt_find_res_for_addr(size_t vaddr);

void print_book(page_t* book, size_t page_n, size_t times);
#endif //CHERIOS_VMEM_H
