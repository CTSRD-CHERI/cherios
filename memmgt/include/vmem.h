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
#include "nanokernel.h"
#include "assert.h"

typedef struct free_chain_t {
    struct used {
        res_t res;
        struct free_chain_t* prev_res;
        struct free_chain_t* next_res;
        act_kt allocated_to;
    } used;
    _Static_assert(RES_USER_SIZE >= sizeof(struct used), "We need space for our metadata");
    char spare[(RES_USER_SIZE - sizeof(struct used))];
} free_chain_t;

extern page_t* book;
extern free_chain_t* chain_start;
extern free_chain_t* chain_end;

/* Sets page_n to cover a range of len (MUST ALREADY BE A VALID RECORD)*/
void break_page_to(size_t page_n, size_t len);

/* Gets the pagen that can be used to index the book. If in doubt, call this.*/
size_t get_valid_page_entry(size_t page_n);

/* Searches for a range of pages of a particular size and minimum length */
size_t find_page_type(size_t required_len, size_t required_type);

/* Get one free page */
size_t get_free_page();

/* Allocates a page table, but finds a physical page for you */
ptable_t memmget_create_table(ptable_t parent, register_t index);

/* Creates a virt->phy mapping but chooses a physical page for you */
int memget_create_mapping(ptable_t L2_table, register_t index);

/* Takes a reservation from the system, i.e. will return a new virtual capability of length */
void memgt_take_reservation(size_t length, act_kt assign_to, cap_pair* out);

/* Calls the nano kernel interface for you and also updates its own bookeeping */
capability memgt_get_phy_page(size_t pagen, register_t cached);

#endif //CHERIOS_VMEM_H
