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


#include "nano/nanokernel.h"
#include "vmem.h"
#include "stdio.h"
#include "math.h"

page_t* book;
free_chain_t *chain_start, *free_chain_start;

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

ptable_t memmgt_create_table(ptable_t parent, register_t index) {
    size_t page = find_page_type(1, page_ptable_free);

    if(page == BOOK_END) page = get_free_page();

    if(page == BOOK_END) return NULL;
    ptable_t r = create_table(page, parent, index);
    try_merge(page);
    return  r;
}

int memmgt_create_mapping(ptable_t L2_table, register_t index, register_t flags) {
    size_t page = find_page_type(2, page_unused);
    if(page == BOOK_END) return -1;

    assert(L2_table != NULL);

    create_mapping(page, L2_table, index, flags);

    try_merge(page);
    return 0;
}

/* TODO commiting per page is a stupid policy. We are doing this for now to make sure everything works */
void memgt_commit_vmem(act_kt activation, size_t addr) {

    ptable_t top_table = get_top_level_table();

    size_t l0_index = L0_INDEX(addr);

    ptable_t l1 = get_sub_table(top_table, l0_index);

    if(l1 == NULL) {
        printf("memmgt: creating a l1 table at index %lx\n", l0_index);
        l1 = memmgt_create_table(top_table, l0_index);
        assert(l1 != NULL);
    }

    size_t l1_index = L1_INDEX(addr);

    ptable_t l2 = get_sub_table(l1, l1_index);

    if(l2 == NULL) {
        printf("memmgt: creating a l2 table at index %lx\n", l1_index);
        l2 = memmgt_create_table(top_table, l1_index);
        assert(l2 != NULL);
    }

    // TODO check not already commited. We may get a few messages.
    memmgt_create_mapping(l2, L2_INDEX(addr), TLB_FLAGS_DEFAULT);
}

void memmgt_free_mapping(ptable_t parent_handle, readable_table_t* parent_ro, size_t index, size_t is_last_level) {
    size_t page_n = parent_ro->entries[index];

    page_n = is_last_level ? (page_n >> PFN_SHIFT) : (PHY_ADDR_TO_PAGEN(page_n));

    break_page_to(page_n, is_last_level ? 2 : 1); // We can't reclaim the page unless we size the record first

    free_mapping(parent_handle, index);

    try_merge(page_n);
}

int range_is_free(readable_table_t *tbl, size_t start, size_t stop) {
    for(size_t i = start; i < stop; i++) {
        if(tbl->entries[i] != VTABLE_ENTRY_USED) return 0;
    }
    return 1;
}

/* Will free a number of mappings, returns how many pages have been freed (might already be free). */
size_t memmgt_free_mappings(ptable_t table, size_t l0, size_t l1, size_t l2, size_t n, size_t lvl, int* can_free) {
    if(can_free) *can_free = 0;

    if(table == NULL) {
        assert(lvl != 0);
        if(lvl == 1) return (PAGE_TABLE_ENT_PER_TABLE - l2) +
                            (PAGE_TABLE_ENT_PER_TABLE * (PAGE_TABLE_ENT_PER_TABLE - l1 - 1));
        if(lvl == 2) return (PAGE_TABLE_ENT_PER_TABLE - l2);
    }


    readable_table_t* RO = get_read_only_table(table);

    size_t ndx = lvl == 0 ? l0 : (lvl == 1 ? l1 : l2);

    int begin_free = can_free && range_is_free(RO, 0, ndx);

    size_t freed = 0;
    while(n > 0) {
        if(lvl == 2) {
            /* Free pages */
            memmgt_free_mapping(table, RO, ndx, 1);
            n -=1;
            freed +=1;
        } else {
            int should_free;
            size_t f = memmgt_free_mappings(get_sub_table(table, ndx), l0, l1, l2, n, lvl+1, &should_free);

            if(should_free) {
                /* Free tables */
                memmgt_free_mapping(table, RO, ndx, 0);
            }
            n -= f;
            freed +=f;
            l2 = 0;
            l1 = 0;
        }
        ndx++;
    }

    if(begin_free) {
        *can_free = range_is_free(RO, ndx, PAGE_TABLE_ENT_PER_TABLE);
    }

    return freed;
}

/* Will free n pages staring at vaddr_start, also freeing tables as required */
void memmgt_free_range(size_t vaddr_start, size_t pages) {

    if(pages == 0) return;

    memmgt_free_mappings(get_top_level_table(),
                         L0_INDEX(vaddr_start), L1_INDEX(vaddr_start), L2_INDEX(vaddr_start),
                         pages, 0, NULL);
}

free_chain_t* memmgt_find_res_for_addr(size_t vaddr) {
    free_chain_t* chain = chain_start;

    do {
        capability nfo = rescap_info(chain->used.res);

        assert(nfo != NULL);

        size_t base = cheri_getbase(nfo);
        size_t length = cheri_getlen(nfo);

        if(base <= vaddr && base+length > vaddr) {
            return chain;
        }

        chain = chain->used.next_res;
    } while(chain != NULL);

    assert(0 && "Chain structure broken");
}

#define CHAIN_FREED  ((act_kt)-1)

static int chain_is_free(free_chain_t* chain) {
    if(chain == NULL) return 0;
    return chain->used.allocated_to == CHAIN_FREED;
}

static void memmgt_merge_res(free_chain_t* a, free_chain_t* b) {
    res_t res_a = a->used.res;
    res_t res_b = b->used.res;

    rescap_merge(res_a, res_b);

    // b is no longer valid for pretty much anything - remove it from the chain

    free_chain_t* next = b->used.next_res;

    a->used.next_res = next;
    if(next != NULL) next->used.prev_res = a;
}

free_chain_t* memmgt_free_res(free_chain_t* chain) {
    chain->used.allocated_to = CHAIN_FREED;

    free_chain_t* pr = chain->used.prev_res;
    free_chain_t* nx = chain->used.next_res;

    capability mid_nfo = rescap_info(chain->used.res);

    size_t true_base = cheri_getbase(mid_nfo) - RES_META_SIZE;
    size_t true_bound = true_base + RES_META_SIZE + cheri_getlen(mid_nfo);

    size_t aligned_base = align_up_to(true_base, PAGE_SIZE);
    size_t aligned_bound = align_down_to(true_bound, PAGE_SIZE);

    if(chain_is_free(pr)) {

        /* Merging might have gained us a page back */
        if(aligned_base != true_base) {
            capability pr_nfo = rescap_info(pr->used.res);
            size_t pr_base = align_up_to(cheri_getbase(pr_nfo) - RES_META_SIZE, PAGE_SIZE);
            if(pr_base != aligned_base) aligned_base-= PAGE_SIZE;
        }

        memmgt_merge_res(pr, chain);
        chain = pr;
    }

    if(chain_is_free(nx)) {

        if(true_bound != aligned_bound) {
            capability  nx_nfo = rescap_info(nx->used.next_res);
            size_t nx_len = RES_META_SIZE + cheri_getlen(nx_nfo);
            if(nx_len >= (true_bound-aligned_bound)) aligned_bound+=PAGE_SIZE;
        }
        memmgt_merge_res(chain, nx);
    }

    size_t pages_to_free = (true_bound-true_base) >> PHY_PAGE_SIZE_BITS;

    printf("Finished freeing reservation over %lx to %lx. Will now free %lx pages\n", true_base, true_bound, pages_to_free);

    if(pages_to_free != 0) memmgt_free_range(aligned_base, pages_to_free);

    return chain;
}



free_chain_t* memmgt_find_free_reservation(size_t with_addr, size_t req_length, size_t* out_base, size_t *out_length) {
    free_chain_t* chain = free_chain_start;

    int care_about_addr = with_addr != 0;
    while(chain != NULL) {

        capability info = rescap_info(chain->used.res);

        size_t base = cheri_getbase(info);
        size_t length = cheri_getlen(info);

        if(care_about_addr) {
            if(base <= with_addr && (length-(with_addr-base)) >= req_length) {
                if(out_base) *out_base = base;
                if(out_length) *out_length = length;
                return chain;
            } else if(base >= with_addr) {
                return NULL;
            }
        } else if(length > req_length) {
            if(out_base) *out_base = base;
            if(out_length) *out_length = length;
            return chain;
        }

        chain = chain->used.next_free_res;

    }

    assert(0 && "We are either completely out of VMEM or this is broken");
}

free_chain_t* memmgt_split_free_reservation(free_chain_t* chain, size_t length) {
    assert((length & (RES_META_SIZE-1)) == 0);

    res_t old = chain->used.res;
    res_t new = rescap_split(old, length);
    free_chain_t* new_chain = (free_chain_t*)get_userdata_for_res(new);

    new_chain->used.res = new;
    new_chain->used.allocated_to = chain->used.allocated_to;
    new_chain->used.next_res = chain->used.next_res;
    new_chain->used.next_free_res = chain->used.next_free_res;

    chain->used.next_res = new_chain;
    chain->used.next_free_res = new_chain;

    if(new_chain->used.next_free_res != NULL) {
        new_chain->used.next_free_res->used.prev_free_res = new_chain;
    }

    if(new_chain->used.next_res != NULL) {
        new_chain->used.next_res->used.prev_res = new_chain;
    }

    return new_chain;
}

static void update_chain(free_chain_t* chain, act_kt assign_to) {
    chain->used.allocated_to = assign_to;

    if(chain->used.prev_free_res != NULL) chain->used.prev_free_res->used.next_free_res = chain->used.next_free_res;
    if(chain->used.next_free_res != NULL) chain->used.next_free_res->used.prev_free_res = chain->used.prev_free_res;

    if(free_chain_start == chain) {
        if(chain->used.prev_free_res != NULL) free_chain_start = chain->used.prev_free_res;
        else free_chain_start = chain->used.next_free_res;
    }

    chain->used.prev_free_res = NULL;
    chain->used.next_free_res = NULL;
}

void memgt_take_reservation(free_chain_t* chain, act_kt assign_to, cap_pair* out) {

    update_chain(chain, assign_to);

    res_t old = chain->used.res;

    rescap_take(old, out);
}

res_t memmgt_parent_reservation(free_chain_t* chain, act_kt assign_to) {
    update_chain(chain, assign_to);

    return rescap_parent(chain->used.res);
}