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
free_chain_t first_reservation;

static inline void print_page(page_t* book, size_t page_n, size_t times) {
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

void set_pages_state(size_t page_n, size_t len, e_page_status from_status, e_page_status to_status) {
    size_t n = get_valid_page_entry(page_n);

    assert(book[n].len >= len);
    assert(book[n].status == from_status);

    if(book[n].len > len) {
        break_page_to(n, len);
    }

    book[n].status = to_status;

    // TODO seetting state may allow us to merge a record
    // TODO or maybe we should do this as we search through the structure
}

size_t find_page_type(size_t required_len, size_t required_type) {
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
    return create_table(page, parent, index);
}

int memget_create_mapping(ptable_t L2_table, register_t index) {
    size_t page = find_page_type(2, page_unused);
    if(page == BOOK_END) return -1;

    create_mapping(page, L2_table, index);

    return 0;
}

capability memgt_take_reservation(size_t length) {
    // TODO we are completely losing track of reservations because malloc doesn't seem to be here
    // TODO add a flag that will mean return the reservation rather than the virt cap itself
    res_t old = first_reservation.reservation;
    first_reservation.reservation = rescap_split(old, length);
    capability p = rescap_take(old);

    return p;
}

capability memgt_get_phy_page(size_t pagen, register_t cached) {
    pagen = get_valid_page_entry(pagen);
    assert(book[pagen].status == page_unused);
    break_page_to(pagen, 1);
    book[pagen].status = page_system_owned;
    return get_phy_page(pagen, cached);
}