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

#ifndef CHERIOS_PMEM_H
#define CHERIOS_PMEM_H

#include "mman.h"

extern page_t* book;

/* Sets page_n to cover a range of len (MUST ALREADY BE A VALID RECORD)*/
void pmem_break_page_to(size_t page_n, size_t len);

/* Gets the pagen that can be used to index the book. If in doubt, call this.*/
size_t pmem_get_valid_page_entry(size_t page_n);

/* One page at a time can be skipped when searching */
void block_finding_page(size_t pagen);
void unblock_finding_page(void);

/* Searches for a range of pages of a particular size and minimum length */
/* If align_precision is true then a range will be returned that will meet cheri precision requirements */
size_t pmem_find_page_type(size_t required_len, e_page_status required_type, int precise);

/* Get one free page */
size_t pmem_get_free_page();

/* Prints out the physical page book for debug */
void pmem_print_book(page_t *book, size_t page_n, size_t times);

/* Debug check a single page */
void pmem_check_phy_entry(size_t pagen);

/* Debug check the book */
void pmem_check_book(void);

/* Tries to remove page_n if it is a redundent node - returns a node that will include it*/
size_t pmem_try_merge(size_t page_n);

/* Get a physical capability. Mediates access to the similar nano kernel function */
void __get_physical_capability(size_t base, size_t length, int IO, int cached, mop_t mop_sealed, cap_pair* result);

/* Dumps the whole book */
void full_dump(void);

/* The clean loop for the cleaner */
void clean_loop(void);

#endif //CHERIOS_PMEM_H
