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


#include "vmem.h"
#include "stdio.h"

page_t* book;
free_chain_t *chain_start, *chain_end;

static void try_merge(size_t page_n) {
    size_t before = book[page_n].prev;
    size_t after = page_n + book[page_n].len;

    if(book[before].status == book[page_n].status) {
        merge_phy_page_range(before);
        page_n = before;
    }

    if((after != BOOK_END) && book[page_n].status == book[after].status) {
        merge_phy_page_range(page_n);
    }
}

void print_book(page_t* book, size_t page_n, size_t times) {
    while(times-- > 0) {
        printf("page: %lx. state = %d. len = %lx. prev = %lx\n",
               page_n,
               book[page_n].status,
               book[page_n].len,
               book[page_n].prev);
        page_n = book[page_n].len + page_n;
        if(page_n == BOOK_END) break;
    }

}

void break_page_to(size_t page_n, size_t len) {
    assert(book[page_n].len > len && len != 0);
    split_phy_page_range(page_n, len);
}

size_t get_valid_page_entry(size_t page_n) {
    assert(page_n < TOTAL_PHY_PAGES);

    if(book[page_n].len != 0) return page_n;

    size_t search_index = 0;

    // TODO here is where a skip list comes in handy.
    // TODO we could also scan linearly, but this is effectively the finest grain of the skip list
    // TODO we could also compact the structure here
    while(book[search_index].len + search_index < page_n) {
        search_index = search_index + book[search_index].len;
    }

    // We had one very large page we need to break. Otherwise we would have returned it straight away
    size_t diff = page_n - search_index;
    break_page_to(search_index, diff);

    return page_n;
}

size_t find_page_type(size_t required_len, e_page_status required_type) {
    size_t search_index = 0;

    while((search_index != BOOK_END) && (book[search_index].len < required_len || book[search_index].status != required_type)) {
        search_index = search_index + book[search_index].len;
    }

    if(search_index == BOOK_END) return BOOK_END;

    if(book[search_index].len != required_len) {
        break_page_to(search_index, required_len);
    }

    return search_index;
}

size_t get_free_page() {
    return find_page_type(1, page_unused);
}

ptable_t memmget_create_table(ptable_t parent, register_t index) {
    size_t page = get_free_page();
    if(page == BOOK_END) return NULL;
    ptable_t r = create_table(page, parent, index);
    try_merge(page);
    return  r;
}

int memget_create_mapping(ptable_t L2_table, register_t index) {
    size_t page = find_page_type(2, page_unused);
    if(page == BOOK_END) return -1;

    create_mapping(page, L2_table, index);

    try_merge(page);
    return 0;
}

/* TODO commiting per page is a stupid policy. We are doing this for now to make sure everything works */
void commit_vmem(act_kt activation, size_t addr) {
    //printf("commiting vaddr %lx:\n", addr);

    ptable_t top_table = get_top_level_table();

    size_t l0_index = L0_INDEX(addr);

    ptable_t l1 = get_sub_table(top_table, l0_index);

    if(l1 == NULL) {
        printf("memmgt: creating a l1 table at index %lx\n", l0_index);
        memmget_create_table(top_table, l0_index);
    }

    size_t l1_index = L1_INDEX(addr);

    ptable_t l2 = get_sub_table(l1, l1_index);

    if(l2 == NULL) {
        printf("memmgt: creating a l2 table at index %lx\n", l1_index);
        memmget_create_table(top_table, l1_index);
    }

    memget_create_mapping(l2, L2_INDEX(addr));
}

void memgt_take_reservation(size_t length, act_kt assign_to, cap_pair* out) {
    /* Have to ask for a length that will keep alignment */
    size_t aligned_length = length;
    size_t mis_align = (length & (RES_META_SIZE-1));
    if(mis_align != 0) {
        aligned_length = length + RES_META_SIZE - mis_align;
    }

    res_t old = chain_end->used.res;
    res_t new = rescap_split(old, aligned_length);
    free_chain_t* chain = (free_chain_t*)get_userdata_for_res(new);

    chain->used.res = new;
    chain->used.allocated_to = NULL;
    chain->used.next_res = NULL;
    chain->used.prev_res = chain_end;
    chain_end->used.next_res = chain;
    chain_end->used.allocated_to = assign_to;

    chain_end = chain;

    rescap_take(old, out);
    out->code = cheri_setbounds(out->code, length);
    out->data = cheri_setbounds(out->data, length);
}