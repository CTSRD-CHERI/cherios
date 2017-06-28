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


#include <nano/nanotypes.h>
#include "syscalls.h"
#include "nano/nanokernel.h"
#include "vmem.h"
#include "stdio.h"
#include "math.h"
#include "string.h"

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
    if(book[page_n].len == len) return;

    if(book[page_n].len > len && len != 0) {
        split_phy_page_range(page_n, len);
    } else {
        printf("len is %lx. Tried to split to %lx\n", book[page_n].len, len);
        assert(0);
    }
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

    assert(book[page].status == page_ptable_free || book[page].status == page_unused);

    ptable_t r = create_table(page, parent, index);

    assert(book[page].status == page_ptable);

    try_merge(page);
    return  r;
}

int memmgt_create_mapping(ptable_t L2_table, register_t index, register_t flags) {
    size_t page = find_page_type(2, page_unused);
    if(page == BOOK_END) return -1;

    assert(L2_table != NULL);

    assert(book[page].status == page_unused);

    create_mapping(page, L2_table, index, flags);

    assert(book[page].status == page_mapped);

    if(page == 0x741c) {
        printf("just mapped problem. index: %lx\n", index << UNTRANSLATED_BITS);
    }

    try_merge(page);
    return 0;
}

/* TODO commiting per page is a stupid policy. We are doing this for now to make sure everything works */
void memgt_commit_vmem(act_kt activation, size_t addr) {

    assert(worker_id == 0);

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
        l2 = memmgt_create_table(l1, l1_index);
        assert(l2 != NULL);
    }

    // TODO check not already commited. We may get a few messages.

    readable_table_t* ro =  get_read_only_table(l2);

    size_t ndx = L2_INDEX(addr);

    if(ro->entries[ndx] != NULL) {
        if(ro->entries[ndx] == VTABLE_ENTRY_USED) {
            panic("Someone tried to use a virtual address that was already freed!\n");
        }
        printf("spurious commit!\n");
    }
    else memmgt_create_mapping(l2, L2_INDEX(addr), TLB_FLAGS_DEFAULT);
}

void memmgt_free_mapping(ptable_t parent_handle, readable_table_t* parent_ro, size_t index, size_t is_last_level) {
    assert(index < PAGE_TABLE_ENT_PER_TABLE);

    size_t page_n = parent_ro->entries[index];

    if(page_n != 0) {
        page_n = is_last_level ? (page_n >> PFN_SHIFT) : (PHY_ADDR_TO_PAGEN(page_n));

        page_n = get_valid_page_entry(page_n);

        break_page_to(page_n, is_last_level ? 2 : 1); // We can't reclaim the page unless we size the record first
    }

    free_mapping(parent_handle, index);

    if(parent_ro->entries[index] != VTABLE_ENTRY_USED) {
        printf("page_n is %lx. index is %lx\n", page_n, index);
    }
    assert(parent_ro->entries[index] == VTABLE_ENTRY_USED);

    if(page_n != 0) {
        try_merge(page_n);
    }
}

/* We should be careful not to free a table that contains an address we are rovking or the nano kernel will complain */
static size_t addr_revoke_low = -1;
static size_t addr_revoke_hi = -1;

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
    while(n > 0 && ndx < PAGE_TABLE_ENT_PER_TABLE) {
        if(lvl == 2) {
            /* Free pages */
            table_entry_t ent = RO->entries[ndx];

            if(ent != VTABLE_ENTRY_USED) {
                memmgt_free_mapping(table, RO, ndx, 1);
            }

            n -=1;
            freed +=1;
        } else {
            int should_free = 1;

            if(lvl == 0) {
                size_t lowest = ndx << (UNTRANSLATED_BITS + L2_BITS + L1_BITS);
                size_t highest = (ndx + 1) << (UNTRANSLATED_BITS + L2_BITS + L1_BITS);

                /* Must not free an L1 page that shares entries with what is being revoked */
                if((lowest <= addr_revoke_low) && (addr_revoke_hi < highest)) should_free = 0;
            } else if(lvl == 1) {
                size_t lowest = l0 << (UNTRANSLATED_BITS + L2_BITS + L1_BITS);
                lowest += ndx << (UNTRANSLATED_BITS + L2_BITS);
                size_t highest = lowest + (1 << (UNTRANSLATED_BITS + L2_BITS));

                /* Must not free an L2 page that shares entries with what is being revoked */
                if((lowest <= addr_revoke_low) && (addr_revoke_hi < highest)) should_free = 0;
            }

            size_t f = memmgt_free_mappings(get_sub_table(table, ndx), ndx, l1, l2, n, lvl+1,
                                            should_free ? &should_free : NULL);

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

static ptable_t get_l2_for_addr(size_t vaddr) {
    ptable_t L0 = get_top_level_table();
    ptable_t L1 = get_sub_table(L0, L0_INDEX(vaddr));
    if(L1 == NULL) return NULL;
    ptable_t L2 = get_sub_table(L1, L1_INDEX(vaddr));
    return L2;
}

static void check_vaddr(size_t vaddr) {
    ptable_t L2 = get_l2_for_addr(vaddr);
    assert(L2 != NULL);
}

static void memmgt_free_single(size_t vaddr) {
    ptable_t l2 = get_l2_for_addr(vaddr);
    assert(l2 != NULL);
    readable_table_t *RO = get_read_only_table(l2);
    memmgt_free_mapping(l2, RO, L2_INDEX(vaddr), 1);
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
        capability nfo;

        if(chain->used.allocated_to != CHAIN_REVOKING) {
            capability foo = chain->used.res;
            assert(foo != NULL);
            nfo = rescap_info(foo);

            assert(nfo != NULL);
        } else {
            nfo = chain->used.res;
        }

        size_t base = cheri_getbase(nfo);
        size_t length = cheri_getlen(nfo);

        if(base <= vaddr && base+length > vaddr) {
            return chain;
        }

        chain = chain->used.next_res;
    } while(chain != NULL);

    printf("Error: could find address %lx in chain\n", vaddr);
    assert(0 && "Chain structure broken");
}

static int chain_is_free(free_chain_t* chain) {
    if(chain == NULL) {
        printf("prev null?\n");
        return 0;
    }
    return chain->used.allocated_to == CHAIN_FREED;
}

static void dump_res(res_t res) {
    capability nfo = rescap_info(res);
    printf("Base: %lx. Length %lx\n", cheri_getbase(nfo)-RES_META_SIZE, cheri_getlen(nfo)+RES_META_SIZE);
}

static void memmgt_merge_res(free_chain_t* a, free_chain_t* b) {
    res_t res_a = a->used.res;
    res_t res_b = b->used.res;

    assert(res_a != NULL);
    assert(res_b != NULL);

    rescap_merge(res_a, res_b);

    assert(a->used.res != NULL);

    // b is no longer valid for pretty much anything - remove it from the chain

    free_chain_t* next = b->used.next_res;

    a->used.next_res = next;
    if(next != NULL) next->used.prev_res = a;
}

free_chain_t* memmgt_free_res(free_chain_t* chain) {
    chain->used.allocated_to = CHAIN_FREED;

    free_chain_t* pr = chain->used.prev_res;
    free_chain_t* nx = chain->used.next_res;

    capability foo = (chain->used.res);
    assert(foo != NULL);
    capability mid_nfo = rescap_info(foo);

    size_t true_base = cheri_getbase(mid_nfo);
    size_t true_bound = true_base + cheri_getlen(mid_nfo);

    size_t aligned_base;
    size_t aligned_bound = align_down_to(true_bound, UNTRANSLATED_PAGE_SIZE);;

    if(chain_is_free(pr)) {

        true_base -= RES_META_SIZE; // If we merge this reservation we will gain back metadata
        aligned_base = align_up_to(true_base, UNTRANSLATED_PAGE_SIZE);

        /* Merging might have gained us a page back */
        if(aligned_base != true_base) {
            capability foo = pr->used.res;
            assert(foo != NULL);
            capability pr_nfo = rescap_info(foo);
            size_t pr_base = align_up_to(cheri_getbase(pr_nfo), UNTRANSLATED_PAGE_SIZE);
            if(pr_base != aligned_base) aligned_base-= UNTRANSLATED_PAGE_SIZE;
        }

        memmgt_merge_res(pr, chain);
        chain = pr;
    } else {
        aligned_base = align_up_to(true_base, UNTRANSLATED_PAGE_SIZE);
    }

    if(chain_is_free(nx)) {

        if(true_bound != aligned_bound) {
            capability foo = nx->used.next_res;
            assert(foo != NULL);
            capability  nx_nfo = rescap_info(foo);
            size_t nx_len = RES_META_SIZE + cheri_getlen(nx_nfo);
            if(nx_len >= (true_bound-aligned_bound)) aligned_bound+=UNTRANSLATED_PAGE_SIZE;
        }
        memmgt_merge_res(chain, nx);
    }

    size_t pages_to_free = (aligned_bound-aligned_base) >> UNTRANSLATED_BITS;

    if(pages_to_free != 0) {
        memmgt_free_range(aligned_base, pages_to_free);
    }

    return chain;
}



free_chain_t* memmgt_find_free_reservation(size_t with_addr, size_t req_length, size_t* out_base, size_t *out_length) {
    free_chain_t* chain = free_chain_start;

    int care_about_addr = with_addr != 0;
    while(chain != NULL) {
        capability foo = chain->used.res;
        assert(foo != NULL);
        capability info = rescap_info(foo);

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

    assert(new != NULL);

    free_chain_t* new_chain = (free_chain_t*)get_userdata_for_res(new);

    new_chain->used.res = new;

    assert(new_chain->used.res != NULL);

    new_chain->used.allocated_to = chain->used.allocated_to;
    new_chain->used.next_res = chain->used.next_res;
    new_chain->used.next_free_res = chain->used.next_free_res;
    new_chain->used.prev_res = chain;
    new_chain->used.prev_free_res = chain;

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

    if(old == NULL) {
        printf("Tried to take a null rervation. Chain data: %p, %d\n", chain, (int)chain->used.allocated_to);
        CHERI_PRINT_CAP(chain->used.prev_res);
        CHERI_PRINT_CAP(chain->used.next_res);
        CHERI_PRINT_CAP(chain->used.prev_free_res);
        CHERI_PRINT_CAP(chain->used.next_free_res);
        CHERI_PRINT_CAP(chain->used.res);
        CHERI_PRINT_CAP(chain);
        CHERI_PRINT_CAP(old);
    }

    assert(old != NULL);

    rescap_take(old, out);
}

res_t memmgt_parent_reservation(free_chain_t* chain, act_kt assign_to) {
    update_chain(chain, assign_to);

    return rescap_parent(chain->used.res);
}

static int mapping_exists(size_t vaddr) {
    ptable_t l2 = get_l2_for_addr(vaddr);
    if(l2 == NULL)
        return 0;
    readable_table_t* ro = get_read_only_table(l2);
    table_entry_t ent = ro->entries[L2_INDEX(vaddr)];

    if(ent == 0 || ent == VTABLE_ENTRY_USED) return 0;
    return 1;
}

static void check_range(size_t vaddr, size_t length) {
    for(size_t page = vaddr; page < vaddr+length; page+=UNTRANSLATED_PAGE_SIZE) {
        if(mapping_exists(vaddr)) {
            printf("vaddr %lx still mapped!\n", vaddr);
            assert(0);
        };
    }
}

static void dump_table(ptable_t tbl) {
    readable_table_t* RO = get_read_only_table(tbl);
    for(int i = 0; i < PAGE_TABLE_ENT_PER_TABLE; i++) {
        printf("%x: %lx\n", i, RO->entries[i]);
    }
}


/* We need to replace the link in the chain being revoked because pointers to it will be revoked! */
static void replace_chain_link(free_chain_t* chain, free_chain_t* free_node) {
    memcpy(free_node, chain, sizeof(free_chain_t));

    free_chain_t* prv = free_node->used.prev_res;
    free_chain_t* nxt = free_node->used.next_res;

    if(prv) prv->used.next_res = free_node;
    if(nxt) nxt->used.prev_res = free_node;
}

void memmgt_revoke_loop(void) {
    while(1) {
        // find a res
        free_chain_t* chain = chain_start;
        free_chain_t* longest = NULL;

        size_t greatest_len = 0;

        while(chain != NULL) {
            if(chain->used.allocated_to == CHAIN_FREED) {
                res_t  res = chain->used.res;
                capability nfo = rescap_info(res);
                size_t base = cheri_getbase(nfo) - RES_META_SIZE;
                size_t len = cheri_getbase(nfo) + RES_META_SIZE;

                if(len > greatest_len) {
                    longest = chain;
                    greatest_len = len;
                }

            }

            chain = chain->used.next_res;
        }

        chain = longest;

        if(chain == NULL) {
            sleep(0);
            sleep(0);
            sleep(0);
            sleep(0);
            continue;
        }

        res_t  res = chain->used.res;

        assert(chain->used.allocated_to == CHAIN_FREED);

        chain->used.allocated_to = CHAIN_REVOKING;

        free_chain_t tmp_link;
        replace_chain_link(chain, &tmp_link);
        /* Store info instead of normal res as the res will be untagged and no longer useable */
        tmp_link.used.res = rescap_info(tmp_link.used.res);

        // revoke a res
        capability nfo = rescap_info(res);
        size_t base = cheri_getbase(nfo) - RES_META_SIZE;
        size_t len = cheri_getlen(nfo) + RES_META_SIZE;

        printf("Revoke: Revoking from %lx to %lx (%lx pages)\n", base, base+len, len/UNTRANSLATED_PAGE_SIZE);

        assert((base & (UNTRANSLATED_PAGE_SIZE-1)) == 0);
        assert((len & (UNTRANSLATED_PAGE_SIZE-1)) == 0);

        addr_revoke_low = base;
        addr_revoke_hi = base+len;

        rescap_revoke_start(res); // reads info from the servation;
        memmgt_free_single(base);

        check_range(base, len);

        check_vaddr(base);
        check_vaddr(base + len - UNTRANSLATED_PAGE_SIZE);

        //dump_table(get_l2_for_addr(base));

        res = rescap_revoke_finish();

        printf("Revoke: Revoke finished!\n");

        assert(res != NULL);

        // update metadata

        printf("Revoke: Getting new chain\n");

        chain = (free_chain_t*)get_userdata_for_res(res);

        printf("Revoke: Fixing metadata\n");

        assert(chain != NULL);

        chain->used.res = res;

        chain->used.allocated_to = NULL;

        free_chain_t *prv, *nxt;


        prv = tmp_link.used.prev_res;
        nxt = tmp_link.used.next_res;

        while(nxt != NULL && nxt->used.allocated_to != NULL) {
            nxt = nxt->used.next_res;
        }

        if(nxt) prv = nxt->used.prev_free_res; // If we found a free we can follow it back

        while(prv != NULL && prv->used.allocated_to != NULL) {
            prv = prv->used.prev_res;
        }

        chain->used.prev_free_res = prv;
        chain->used.next_free_res = nxt;

        if(prv) prv->used.next_free_res = chain;
        if(nxt) nxt->used.prev_free_res = chain;

        printf("************Revoke restart\n");
    }
}