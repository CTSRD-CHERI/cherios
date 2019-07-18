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
#include "object.h"

ptable_t vmem_create_table(ptable_t parent, register_t index, __unused int level) { // FIXME: Races with main thread
    //printf("memmgt: creating a l%d table at index %lx\n", level, index);
    size_t page = pmem_find_page_type(1, page_ptable_free, PMEM_BACKWARDS, BOOK_END);

    if(page == BOOK_END) page = pmem_find_page_type(1, page_unused, PMEM_BACKWARDS, BOOK_END);

    if(page == BOOK_END) return NULL;

    assert(book[page].status == page_ptable_free || book[page].status == page_unused);

    ptable_t r = create_table(page, parent, index);

    if(book[page].status != page_ptable) {
        printf("Trying to use %lx as a table %lx %d\n", page, book[page].len, book[page].status);
        full_dump();
    }

    assert(book[page].status == page_ptable);

    pmem_check_phy_entry(page);

    pmem_try_merge(page);

    assert(r != NULL);

    return  r;
}

int vmem_create_mapping(ptable_t L2_table, register_t index, register_t flags) { // FIXME: Races with other thread

    size_t page = pmem_find_page_type(2, page_unused, PMEM_NONE, 0);
    if(page == BOOK_END) return -1;

    assert(L2_table != NULL);

    assert(book[page].status == page_unused);

    create_mapping(page, L2_table, index, index + 1, flags);

    assert(book[page].status == page_mapped);

    pmem_check_phy_entry(page);

    pmem_try_merge(page);
    return 0;
}

/* TODO commiting per page is a stupid policy. We are doing this for now to make sure everything works */
void vmem_commit_vmem(act_kt activation, char* name, size_t addr) {
    // WARN: THIS MUST NOT TOUCH VIRTUAL MEMORY (that has not been commited).
    // Mops are virtual, if we want to update commit tallies, send a message.

    assert(worker_id == 0);

    if(addr < 0x2000) {
        printf("Commit for %lx\n", addr);
    }
    ptable_t top_table = get_top_level_table();

    size_t l0_index = L0_INDEX(addr);

    ptable_t l1 = get_sub_table(top_table, l0_index);

    if(l1 == NULL) {
        l1 = vmem_create_table(top_table, l0_index, 1);
    }

    size_t l1_index = L1_INDEX(addr);

    ptable_t l2 = get_sub_table(l1, l1_index);

    if(l2 == NULL) {
        l2 = vmem_create_table(l1, l1_index, 2);
    }

    // TODO check not already commited. We may get a few messages.

    readable_table_t* ro =  get_read_only_table(l2);

    size_t ndx = L2_INDEX(addr);

    if(ro->entries[ndx] != VTABLE_ENTRY_FREE) {
        if(ro->entries[ndx] == VTABLE_ENTRY_USED) {
            printf("%s used %lx\n", name, addr);
            CHERI_PRINT_CAP(activation);
            panic_proxy("Someone tried to use a virtual address (%lx) that was already freed!\n", activation);
        }
        printf(KRED"spurious commit by %s at vaddr %lx -> %lx!\n"KRST, name, addr, ro->entries[ndx]);
    }
    else vmem_create_mapping(l2, L2_INDEX(addr), TLB_FLAGS_DEFAULT);

    syscall_vmem_notify(activation, msg_queue_empty());
    // TODO bump counters on commits for MOPs
}

size_t __vmem_commit_vmem_range(size_t addr, size_t pages, mem_request_flags flags) {
    assert(worker_id == 0);

    pages <<=1; // How many physical pages this will be

    // DMA wants to have all contiguous physical
    // Otherwise its fine to skip a page of its already commited

    int is_dma = flags & COMMIT_DMA;

    // Get the first vtable entry

    ptable_t top_table = get_top_level_table();
    ptable_t l2, l1;

    size_t l0_index = L0_INDEX(addr) - 1;
    size_t l1_index = L1_INDEX(addr) - 1; // offset by 1 as do while increments by 1
    size_t ndx = L2_INDEX(addr);

    size_t phy_pagen = 0;
    size_t in_block = 0;
    size_t contig_base = 0;

    int first = 1;

    do {

        l1_index++;

        if(l1_index == PAGE_TABLE_ENT_PER_TABLE || first) {
            l0_index++;
            l1 = get_sub_table(top_table, l0_index);
            if(l1 == NULL) l1 = vmem_create_table(top_table, l0_index, 1);
            if(!first) l1_index = 0;
            first = 0;
        }

        assert((pages & 1) == 0);

        l2 = get_sub_table(l1, l1_index);
        if(l2 == NULL) {
            l2 = vmem_create_table(l1, l1_index, 2);
        }

        readable_table_t* ro = get_read_only_table(l2);

        assert(ro);

        // We may have lost a few pages to making tables
        in_block = book[phy_pagen].status == page_unused ? book[phy_pagen].len : 0;

        size_t end_ndx = ndx + (pages / 2);
        end_ndx = end_ndx > PAGE_TABLE_ENT_PER_TABLE ? PAGE_TABLE_ENT_PER_TABLE : end_ndx;

        size_t pages_tmp = pages;

        pages -= (end_ndx-ndx) * 2;

        do {
            // Loop to find unmapped range
            while(ndx != end_ndx && ro->entries[ndx] != VTABLE_ENTRY_FREE) ndx++;

            size_t end_empty = ndx;

            if(ndx == end_ndx) break;

            while(end_empty != end_ndx && ro->entries[end_empty] == VTABLE_ENTRY_FREE) end_empty++;

            // For DMA must not find any mappings
            if(is_dma) assert(end_empty == end_ndx);

            while(ndx != end_empty) {
                // commit  [ndx,end_empty)
                if(in_block < 2) {
                    if(is_dma) assert(contig_base == 0);
                    phy_pagen = pmem_find_page_type(is_dma ? pages_tmp : 2, page_unused, PMEM_ALLOW_GREATER, phy_pagen);
                    assert(phy_pagen != BOOK_END);
                    contig_base = contig_base == 0 ? phy_pagen : (size_t)(-1);
                    in_block = book[phy_pagen].len;
                }

                size_t phy_len = (end_empty - ndx) * 2;

                if(phy_len > in_block) phy_len = in_block;

                if(in_block != phy_len) {
                    pmem_break_page_to(phy_pagen, phy_len);
                }

                size_t block_end_ndx = ndx + (phy_len/2);

                assert(book[phy_pagen].status == page_unused);
                create_mapping(phy_pagen, l2, ndx, block_end_ndx, (flags & COMMIT_UNCACHED) ? TLB_FLAGS_UNCACHED : TLB_FLAGS_DEFAULT);
                assert(book[phy_pagen].status == page_mapped);

                size_t prev = book[phy_pagen].prev;
                if(book[prev].status == page_mapped) merge_phy_page_range(prev);

                in_block -= phy_len;
                phy_pagen+= phy_len;
                ndx = block_end_ndx;

                if(in_block == 0 && book[phy_pagen].status == page_mapped) {
                    size_t next = book[phy_pagen].len + phy_pagen;
                    merge_phy_page_range(book[phy_pagen].prev);
                    phy_pagen = next;
                }
            }

            ndx = end_empty+1;

        } while(ndx < end_ndx);

        ndx = 0;

    } while(pages);

    return (contig_base << PHY_PAGE_SIZE_BITS) + (RES_META_SIZE);
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

    if(is_last_level) {
        if(clean_notify != NULL) {
            act_notify_kt tmp = clean_notify;
            clean_notify = NULL;
            syscall_cond_notify(tmp);
        }
    }
}

int range_is_free(readable_table_t *tbl, size_t start, size_t stop) {
    for(size_t i = start; i < stop; i++) {
        if(tbl->entries[i] != VTABLE_ENTRY_USED) return 0;
    }
    return 1;
}

/* Will free a number of mappings, returns how many pages have been freed (might already be free). */
/* We don't need to take care of revoking ranges here as we always leave the first and last page mapped */
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

            size_t f = memmgt_free_mappings(get_sub_table(table, ndx), ndx, l1, l2, n, lvl+1,
                                            should_free ? &should_free : NULL);

            if(should_free) {
                /* Free tables */
                memmgt_free_mapping(table, RO, ndx, 0);
            } else {
                begin_free = 0; // If you can't free the current entry the page is not free
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

__unused static void dump_res(res_t res) {
    res_nfo_t nfo = rescap_nfo(res);
    printf("Base: %lx. Length %lx\n", nfo.base-RES_META_SIZE, nfo.length+RES_META_SIZE);
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

__unused static void check_range(size_t vaddr, size_t length) {
    for(size_t page = vaddr; page < vaddr+length; page+=UNTRANSLATED_PAGE_SIZE) {
        if(mapping_exists(vaddr)) {
            printf("vaddr %lx still mapped!\n", vaddr);
            assert(0);
        };
    }
}

__unused static void dump_table(ptable_t tbl) {
    readable_table_t* RO = get_read_only_table(tbl);
    for(int i = 0; i < PAGE_TABLE_ENT_PER_TABLE; i++) {
        printf("%x: %lx\n", i, RO->entries[i]);
    }
}

void vmem_visit_range(size_t page_start, size_t pages, vmem_visit_func* func, capability arg) {

    while(pages != 0) {
        size_t l0_index = L0_INDEX(page_start << UNTRANSLATED_BITS);
        ptable_t L0 = get_top_level_table();
        readable_table_t* RO = get_read_only_table(L0);
        register_t state = RO->entries[l0_index];

        if(state == VTABLE_ENTRY_USED || state == VTABLE_ENTRY_TRAN || state == 0) {
            size_t reps = PAGE_TABLE_ENT_PER_TABLE*PAGE_TABLE_ENT_PER_TABLE;
            func(arg, L0, RO, l0_index, reps);
            page_start += reps;
            pages = pages < reps ? 0 : pages - reps;
            continue;
        }

        size_t l1_index = L1_INDEX(page_start << UNTRANSLATED_BITS);
        ptable_t L1 = get_sub_table(L0, l0_index);
        RO = get_read_only_table(L1);

        state = RO->entries[l1_index];

        if(state == VTABLE_ENTRY_USED || state == VTABLE_ENTRY_TRAN || state == 0) {
            size_t reps = PAGE_TABLE_ENT_PER_TABLE;
            func(arg, L1, RO, l1_index, reps);
            page_start += reps;
            pages = pages < reps ? 0 : pages - reps;
            continue;
        }

        size_t l2_index = L2_INDEX(page_start << UNTRANSLATED_BITS);
        ptable_t L2 = get_sub_table(L1, l1_index);
        RO = get_read_only_table(L2);

        func(arg, L2, RO, l2_index, 1);
        page_start++;
        pages--;
    }

}

size_t virtual_to_physical(size_t vaddr) {
    assert(0 && "Depracated");

    size_t low_bits = vaddr & (UNTRANSLATED_PAGE_SIZE-1);
    ptable_t tbl = get_l2_for_addr(vaddr);
    if(tbl == NULL) return 0;

    readable_table_t* RO = get_read_only_table(tbl);

    size_t PFN = RO->entries[L2_INDEX(vaddr)];

    size_t page_no = (PFN >> PFN_SHIFT);

    // This really should be plus. There is one bit of overlap between the low bits and the last bit from page_no
    return (page_no << PHY_PAGE_SIZE_BITS) + low_bits;
}