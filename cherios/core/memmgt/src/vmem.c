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
#include "pmem.h"

ptable_t vmem_create_table(ptable_t parent, register_t index) {
    size_t page = pmem_find_page_type(1, page_ptable_free);

    if(page == BOOK_END) page = pmem_get_free_page();

    if(page == BOOK_END) return NULL;

    assert(book[page].status == page_ptable_free || book[page].status == page_unused);

    ptable_t r = create_table(page, parent, index);

    assert(book[page].status == page_ptable);

    pmem_check_phy_entry(page);

    pmem_try_merge(page);
    return  r;
}

int vmem_create_mapping(ptable_t L2_table, register_t index, register_t flags) {
    size_t page = pmem_find_page_type(2, page_unused);
    if(page == BOOK_END) return -1;

    assert(L2_table != NULL);

    assert(book[page].status == page_unused);

    create_mapping(page, L2_table, index, flags);

    assert(book[page].status == page_mapped);

    pmem_check_phy_entry(page);

    pmem_try_merge(page);
    return 0;
}

/* TODO commiting per page is a stupid policy. We are doing this for now to make sure everything works */
void vmem_commit_vmem(act_kt activation, char* name, size_t addr) {
    // WARN: THIS MUST NOT TOUCH VIRTUAL MEMORY. Mops are virtual, if we want to update commit tallies, send a message.

    assert(worker_id == 0);

    if(addr < 0x2000) {
        printf("Commit for %lx\n", addr);
    }
    ptable_t top_table = get_top_level_table();

    size_t l0_index = L0_INDEX(addr);

    ptable_t l1 = get_sub_table(top_table, l0_index);

    if(l1 == NULL) {
        printf("memmgt: creating a l1 table at index %lx\n", l0_index);
        l1 = vmem_create_table(top_table, l0_index);
        assert(l1 != NULL);
    }

    size_t l1_index = L1_INDEX(addr);

    ptable_t l2 = get_sub_table(l1, l1_index);

    if(l2 == NULL) {
        printf("memmgt: creating a l2 table at index %lx\n", l1_index);
        l2 = vmem_create_table(l1, l1_index);
        assert(l2 != NULL);
    }

    // TODO check not already commited. We may get a few messages.

    readable_table_t* ro =  get_read_only_table(l2);

    size_t ndx = L2_INDEX(addr);

    if(ro->entries[ndx] != NULL) {
        if(ro->entries[ndx] == VTABLE_ENTRY_USED) {
            printf("%s used %lx\n", name, addr);
            CHERI_PRINT_CAP(activation);
            panic_proxy("Someone tried to use a virtual address (%lx) that was already freed!\n", activation);
        }
        printf("spurious commit by %s at vaddr %lx!\n", name, addr);
    }
    else vmem_create_mapping(l2, L2_INDEX(addr), TLB_FLAGS_DEFAULT);

    // TODO bump counters on commits for MOPs
}

void memmgt_free_mapping(ptable_t parent_handle, readable_table_t* parent_ro, size_t index, size_t is_last_level) {
    assert(index < PAGE_TABLE_ENT_PER_TABLE);

    size_t page_n = parent_ro->entries[index];

    if(page_n != 0) {
        page_n = is_last_level ? (page_n >> PFN_SHIFT) : (PHY_ADDR_TO_PAGEN(page_n));

        page_n = pmem_get_valid_page_entry(page_n);

        pmem_break_page_to(page_n, is_last_level ? 2 : 1); // We can't reclaim the page unless we size the record first
    }

    free_mapping(parent_handle, index);

    if(parent_ro->entries[index] != VTABLE_ENTRY_USED) {
        printf("page_n is %lx. index is %lx\n", page_n, index);
    }
    assert(parent_ro->entries[index] == VTABLE_ENTRY_USED);

    if(page_n != 0) {
        pmem_try_merge(page_n);
    }
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

            } else if(lvl == 1) {
                size_t lowest = l0 << (UNTRANSLATED_BITS + L2_BITS + L1_BITS);
                lowest += ndx << (UNTRANSLATED_BITS + L2_BITS);
                size_t highest = lowest + (1 << (UNTRANSLATED_BITS + L2_BITS));
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

void vmem_free_single(size_t vaddr) {
    ptable_t l2 = get_l2_for_addr(vaddr);
    assert(l2 != NULL);
    readable_table_t *RO = get_read_only_table(l2);
    memmgt_free_mapping(l2, RO, L2_INDEX(vaddr), 1);
}

/* Will free n pages staring at vaddr_start, also freeing tables as required */
void vmem_free_range(size_t vaddr_start, size_t pages) {

    if(pages == 0) return;

    memmgt_free_mappings(get_top_level_table(),
                         L0_INDEX(vaddr_start), L1_INDEX(vaddr_start), L2_INDEX(vaddr_start),
                         pages, 0, NULL);
}

static void dump_res(res_t res) {
    capability nfo = rescap_info(res);
    printf("Base: %lx. Length %lx\n", cheri_getbase(nfo)-RES_META_SIZE, cheri_getlen(nfo)+RES_META_SIZE);
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

size_t virtual_to_physical(size_t vaddr) {
    size_t low_bits = vaddr & (UNTRANSLATED_PAGE_SIZE-1);
    ptable_t tbl = get_l2_for_addr(vaddr);
    if(tbl == NULL) return 0;

    readable_table_t* RO = get_read_only_table(tbl);

    size_t PFN = RO->entries[L2_INDEX(vaddr)];

    return ((PFN >> PFN_SHIFT) << UNTRANSLATED_BITS) | low_bits;
}