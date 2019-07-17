/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include <mmap.h>
#include "sys/mman.h"
#include "types.h"
#include "utils.h"
#include "vmem.h"
#include "math.h"
#include "mmap.h"
#include "syscalls.h"
#include "stdio.h"
#include "string.h"
#include "pmem.h"


mop_internal_t mmap_mop;

/* A small fixed size allocator. We can't use virtual memory so we can't use the normal allocator */

capability mop_sealing_cap;
vpage_range_desc_table_t desc_table_root;

vpage_range_desc_table_t* desc_table_pool;
size_t desc_table_pool_alloc_n = DESC_ALLOC_N_PER_POOL;

vpage_range_desc_table_t* next_free_desc_table = NULL;
size_t n_desc_tables_used = 0;

static void init_desc_table(vpage_range_desc_table_t* table) {
    bzero(table, sizeof(vpage_range_desc_table_t));
}

static vpage_range_desc_table_t* desc_table_alloc(void) {
    vpage_range_desc_table_t* res = next_free_desc_table;

    if(res != NULL) {
        next_free_desc_table = res->next_free;
    } else {

        if(desc_table_pool_alloc_n == DESC_ALLOC_N_PER_POOL) {
            desc_table_pool_alloc_n = 0;
            size_t pagen = pmem_find_page_type(DESC_ALLOC_CHUNK_PAGES, page_unused, PMEM_PRECISE | PMEM_BACKWARDS, BOOK_END);
            assert_int_ex(pagen, !=, BOOK_END);
            cap_pair pr;
            pr.data = NULL;
            get_phy_page(pagen, 1, DESC_ALLOC_CHUNK_PAGES, &pr, 0);
            assert(pr.data != NULL);
            pmem_try_merge(pagen);
            desc_table_pool = pr.data;
        }

        n_desc_tables_used++;
        res = & desc_table_pool[desc_table_pool_alloc_n++];

        init_desc_table(res);
    }

    return res;
}

static void desc_table_free(vpage_range_desc_table_t* table) {
    table->next_free = next_free_desc_table;
    next_free_desc_table = table;
}

/* These maintain free chains through desciptors so the page allocator doesn't have to walk the structure too much */

#ifdef MAX_POOLS

static vpage_range_desc_t* peek_from_pool_head(size_t pool) {
    return pool_heads[pool];
}

#define PREV_FIXUP(desc) (desc->tracking.free_chain.prev ? &(desc->tracking.free_chain.prev->tracking.free_chain.next) : &pool_heads[desc->tracking.free_chain.pool_id-1])
#define NEXT_FIXUP(desc) (desc->tracking.free_chain.next ? &desc->tracking.free_chain.next->tracking.free_chain.prev : NULL)
#define HAS_NEXT(desc) (desc->tracking.free_chain.next)

static inline void set_not_in_pool(vpage_range_desc_t* desc) {
    assert(desc->allocation_type == open_node || desc->allocation_type == tomb_node);
    desc->tracking.free_chain.pool_id = 0;
}

static inline vpage_range_desc_t* remove_from_pool(vpage_range_desc_t* desc) {
    if(desc) {
        assert(desc->allocation_type == open_node || desc->allocation_type == tomb_node);
        assert(desc->tracking.free_chain.pool_id);
        *PREV_FIXUP(desc) = desc->tracking.free_chain.next;
        if(HAS_NEXT(desc)) *NEXT_FIXUP(desc) = desc->tracking.free_chain.prev;
        desc->tracking.free_chain.pool_id = 0;
    }
    return desc;
}

static inline void add_to_pool(vpage_range_desc_t* desc, size_t pool) {
    assert(desc->allocation_type == open_node || desc->allocation_type == tomb_node);
    desc->tracking.free_chain.next = pool_heads[pool];
    desc->tracking.free_chain.prev = NULL;
    if(pool_heads[pool]) pool_heads[pool]->tracking.free_chain.prev = desc;
    desc->tracking.free_chain.pool_id = pool+1;
    pool_heads[pool] = desc;
}

static inline void add_after_desc(vpage_range_desc_t* prev, vpage_range_desc_t* desc) {
    assert(desc->allocation_type == open_node || desc->allocation_type == tomb_node);
    assert(prev->allocation_type == open_node || prev->allocation_type == tomb_node);
    assert(prev->tracking.free_chain.pool_id);
    desc->tracking.free_chain.pool_id = prev->tracking.free_chain.pool_id;
    desc->tracking.free_chain.next = prev->tracking.free_chain.next;
    desc->tracking.free_chain.prev = prev;
    prev->tracking.free_chain.next = desc;
    if(desc->tracking.free_chain.next) desc->tracking.free_chain.next->tracking.free_chain.prev = desc;
}

static inline void fixup_desc_chain(vpage_range_desc_t* desc) {
    assert(desc->allocation_type == open_node || desc->allocation_type == tomb_node);
    if(desc->tracking.free_chain.pool_id) {
        *PREV_FIXUP(desc) = desc;
        if(HAS_NEXT(desc)) *NEXT_FIXUP(desc) = desc;
    }
}

__unused static vpage_range_desc_t* remove_from_pool_head(size_t pool) {
    return remove_from_pool(pool_heads[pool]);
}

__unused static void check_pool(size_t pool) {
    vpage_range_desc_t* desc = pool_heads[pool];
    vpage_range_desc_t* prev = NULL;
    while(desc) {
        assert(desc->allocation_type == open_node);
        assert(desc->tracking.free_chain.pool_id == pool+1);
        assert(desc->tracking.free_chain.prev == prev);
        assert(desc->tracking.free_chain.next != desc);
        prev = desc;
        desc = desc->tracking.free_chain.next;
    }
}
#endif

/* Intialisation of the map structure. Has two nodes, one a sentinal for the (non existant) first page, and one for the
 * rest of virtual memory */

static void init_desc(res_t big_res) {
    init_desc_table(&desc_table_root);

    vpage_range_desc_table_t* L1 = desc_table_alloc();
    vpage_range_desc_table_t* L2 = desc_table_alloc();

    for(size_t i = 0; i < PAGE_TABLE_ENT_PER_TABLE; i++) {

    }
    desc_table_root.ranges_allocated = 1;
    desc_table_root.pages_allocated = 1; // Sentry page
    desc_table_root.descs[0].allocation_type = internal_node;
    desc_table_root.descs[0].sub_table = L1;
    desc_table_root.descs[0].start = 0;
    desc_table_root.descs[0].length = MAX_VIRTUAL_PAGES; // This node all information
    desc_table_root.descs[0].reservation = big_res;

    L1->pages_allocated = 1; // Sentry page
    L1->descs[0].allocation_type = internal_node;
    L1->descs[0].sub_table = L2;
    L1->descs[0].start = 0;
    L1->descs[0].length = MAX_VIRTUAL_PAGES;
    L1->descs[0].reservation = big_res;

    L2->pages_allocated = 1; // Sentry page

    L2->descs[0].allocation_type = allocation_node; // Fake allocation. Nobody should be able to free this
    L2->descs[0].allocated_length = 1;
    L2->descs[0].start = 0;
    L2->descs[0].length = 1;
    L2->descs[0].prev = 0;

    L2->descs[1].allocation_type = open_node;
    L2->descs[1].start = 1;
    L2->descs[1].length = MAX_VIRTUAL_PAGES - 1;
    L2->descs[1].prev = 0;
    L2->descs[1].reservation = big_res; // This record holds the reservation for all virtual space

    add_to_pool(&L2->descs[1], PAGE_POOL_FREE_LARGE);
}

/* Some utility functions */

#define NDX(N,lvl) (((N) >> (PAGE_TABLE_BITS_PER_LEVEL * (2-(lvl)))) & (PAGE_TABLE_ENT_PER_TABLE -1))

static void mmap_dump_desc(vpage_range_desc_t* desc) {
    if(desc == NULL) {
        printf("NULL\n");
        return;
    }
    printf("\nStart: %lx (%lx). End %lx (%lx). Len %lx (%lx). Prev %lx. State %s.\n",
           desc->start << UNTRANSLATED_BITS,
           desc->start,
           (desc->start + desc->length) << UNTRANSLATED_BITS,
           (desc->start + desc->length),
           desc->length << UNTRANSLATED_BITS,
           desc->length,
           (desc->prev) << UNTRANSLATED_BITS,
           desc->allocation_type == open_node ? "open" : (desc->allocation_type == allocation_node ? "allocation" :
                                                          (desc->allocation_type == internal_node ? "internal" : "tomb")));
    printf("|---Allocation length %lx\n", desc->allocated_length);
    if(desc->allocated_length != 0 || desc->allocation_type == open_node) {
        printf("|---");
        CHERI_PRINT_CAP(desc->reservation);
    }
    if(desc->allocation_type == allocation_node) {
        printf("|---Claimers: \n");
        FOREACH_CLAIMER(desc, index, claim) {
            if(claim->owner != NULL) {
                printf("    |--- %s(%lx) x%ld\n", claim->owner->debug_id, cheri_getcursor(claim->owner), claim->n_claims);
            }
        }
    } else if(desc->allocation_type == open_node || desc->allocation_type == tomb_node) {
        if(desc->tracking.free_chain.pool_id) {
            printf("|---Tracking pool %ld. prev %lx. next %lx\n", desc->tracking.free_chain.pool_id-1, (size_t)desc->tracking.free_chain.prev, (size_t)desc->tracking.free_chain.next);
        }
    }
}

__unused static inline size_t check_desc_correctness(vpage_range_desc_t* desc, size_t expected_prev) {
    if(desc->allocation_type != internal_node) {
        if(expected_prev != desc->prev) {
            printf("This is wrong:\n");
            mmap_dump_desc(desc);
            mmap_dump();
        }
        assert_int_ex(expected_prev, == , desc->prev);
        return desc->start;
    }

    vpage_range_desc_table_t* tbl1 = desc->sub_table;

    size_t minstart = (size_t)(-1);
    size_t maxend = 0;

    for(size_t i = 0; i < PAGE_TABLE_ENT_PER_TABLE; i++) {
        vpage_range_desc_t* sub = & tbl1->descs[i];

        if(sub->length != 0) {
            expected_prev = check_desc_correctness(sub, expected_prev);
            if((size_t)(-1) == minstart) minstart = sub->start;
            maxend = sub->start + sub->length;
        }
    }

    if(minstart != desc->start || maxend != desc->start + desc->length) {
        printf("This is wrong:\n");
        mmap_dump_desc(desc);
        mmap_dump();
    }

    assert_int_ex(minstart, ==, desc->start);
    assert_int_ex(maxend, ==, desc->start + desc->length);

    return expected_prev;
}

// Fast but will only work if page_n is not in the middle a range covered by a node
static vpage_range_desc_t* hard_index(size_t page_n) {

    if(page_n == MAX_VIRTUAL_PAGES) return NULL;

    size_t l0 = L0_INDEX(page_n << UNTRANSLATED_BITS);
    vpage_range_desc_t* l = & desc_table_root.descs[l0];
    if(l->allocation_type != internal_node) {
        return l;
    }

    size_t l1 = L1_INDEX(page_n << UNTRANSLATED_BITS);
    l = & l->sub_table->descs[l1];

    if(l->allocation_type != internal_node) {
        return l;
    }

    size_t l2 = L2_INDEX(page_n << UNTRANSLATED_BITS);
    l = & l->sub_table->descs[l2];

    return l;
}

static size_t find_free_claim_index(vpage_range_desc_t* desc, mop_internal_t* mop) {
    assert_int_ex(desc->allocation_type, ==, allocation_node);
    size_t free_index = (size_t)-1;
    size_t claim_index = (size_t)-1;
    FOREACH_CLAIMER(desc, index, claim) {
        if(claim->owner == NULL) {
            free_index = index;
        }
        if(claim->owner == mop) {
            claim_index = index;
        }
    }

    if(claim_index != (size_t)(-1)) return  claim_index;
    else return free_index;
}

static int claim_node(mop_internal_t* mop, vpage_range_desc_t* desc, size_t index, size_t times) {
    // We are using NULL as no owner
    assert(mop != NULL);
    assert(desc->allocation_type == allocation_node);

    claimer_t* claim = &desc->tracking.claimers[index];
    assert(claim->owner == NULL);

    claimer_link_t old_last = mop->last;

    claim->owner = mop;
    claim->n_claims = times;

    desc->claims_used++;

    MAKE_CLAIM_LINK(mop->last, desc, index);

    if(LINK_IS_END(mop->first)) {
        // Our first claim
        mop->first = mop->last;
        LINK_SET_END(claim->prev);
    } else {
        FOLLOW(old_last).next = mop->last;
        claim->prev = old_last;
    }

    LINK_SET_END(claim->next);

    mop->allocated_pages += desc->length;
    mop->allocated_ranges++;

    return 0;
}

static void release_claim(vpage_range_desc_t* desc, size_t index) {
    assert(desc->allocation_type == allocation_node);

    claimer_t* claim = &desc->tracking.claimers[index];
    mop_internal_t* owner = claim->owner;
    assert(owner != NULL);

    if(LINK_IS_END(claim->prev)) {
        owner->first = claim->next;
    } else {
       FOLLOW(claim->prev).next = claim->next;
    }

    if(LINK_IS_END(claim->next)) {
        owner->last = claim->prev;
    } else {
        FOLLOW(claim->next).prev = claim->prev;
    }

    desc->claims_used--;

    claim->owner = NULL;

    owner->allocated_pages -= desc->length;
    owner->allocated_ranges--;
}

static void transfer_claims(vpage_range_desc_t* desc, vpage_range_desc_t* free_desc) {
    size_t alloc_fix = free_desc->length;
    FOREACH_CLAIMER(desc, index, claim) {
        if(claim->owner != NULL) {
            claim_node(claim->owner, free_desc, index, claim->n_claims);
            claim->owner->allocated_pages -= alloc_fix; // Don't double count
        }
    }
}

// When we move a description claims will point to the wrong node. These will relink things
static void fixup_claims(vpage_range_desc_t* desc) {
    assert(desc->allocation_type == allocation_node);
    for(size_t i = 0; i != MAX_CLAIMERS; i++) {
        if(desc->tracking.claimers[i].owner != NULL) {
            claimer_link_t* link_prev = LINK_IS_END(desc->tracking.claimers[i].prev) ? &desc->tracking.claimers[i].owner->first : &FOLLOW((desc->tracking.claimers[i].prev)).next;
            claimer_link_t* link_next = LINK_IS_END(desc->tracking.claimers[i].next) ? &desc->tracking.claimers[i].owner->last : &FOLLOW((desc->tracking.claimers[i].next)).prev;
            MAKE_CLAIM_LINK((*link_prev),desc,i);
            MAKE_CLAIM_LINK((*link_next),desc,i);
        }
    }
}

struct index_result {
    vpage_range_desc_t* result;
    size_t indexs[3];
};

// Fast but will only work if page_n is not in the middle a range covered by a node
static vpage_range_desc_t* hard_index_parent(size_t page_n, vpage_range_desc_t* child) {

    if(page_n == MAX_VIRTUAL_PAGES) return NULL;

    size_t l0 = L0_INDEX(page_n << UNTRANSLATED_BITS);
    vpage_range_desc_t* l = & desc_table_root.descs[l0];
    if(l->allocation_type != internal_node) {
        return NULL;
    }

    vpage_range_desc_t* parent = l;
    size_t l1 = L1_INDEX(page_n << UNTRANSLATED_BITS);
    l = & l->sub_table->descs[l1];

    if(l->allocation_type != internal_node) {
        assert( l == child);
        return parent;
    }

    size_t l2 = L2_INDEX(page_n << UNTRANSLATED_BITS);
    parent = l;
    l = & l->sub_table->descs[l2];

    assert(l == child);

    return parent;
}

// Will do some searching, but no more than 3 tables worth at the worst
static void soft_index(size_t page_n, struct index_result* result) {
    size_t fast_ndx = NDX(page_n, 0);

    vpage_range_desc_table_t *tbl = & desc_table_root;

    size_t ndx = 0;
    vpage_range_desc_t* desc;

    size_t lvl = 0;
    while(lvl != 3) {
        if(tbl->descs[fast_ndx].length == 0) {
            // This node invalid
            desc = & tbl->descs[ndx];
            size_t end = desc->start + desc->length;

            while(page_n > end) {
                ndx = NDX(end, lvl);
                desc = & tbl->descs[ndx];
                end = desc->start + desc->length;
            }
        } else if (tbl->descs[fast_ndx].start > page_n) {
            // Covered by the previous node
            ndx = fast_ndx;
            desc = & tbl->descs[ndx];
            ndx = NDX(desc->prev, lvl);
            desc = & tbl->descs[ndx];
        } else {
            // Covered by this node
            ndx = fast_ndx;
            desc = & tbl->descs[ndx];
        }

        result->indexs[lvl] = ndx;

        if(desc->allocation_type != internal_node) {
            result->result = desc;
            return;
        }

        lvl++;

        tbl = desc->sub_table;
        ndx = NDX(desc->start, lvl);
        fast_ndx = NDX(page_n, lvl);
    }

    assert(0);
}

/* Visitor functions for request/claim/free */

static int visit_claim_check(vpage_range_desc_t *desc, __unused size_t base, mop_internal_t* owner, __unused size_t length, size_t times) {
    if(desc->allocation_type == open_node) {
        return MEM_CLAIM_NOT_IN_USE;
    } else if(desc->allocation_type == tomb_node) {
        return MEM_CLAIM_FREED;
    }

    size_t index = find_free_claim_index(desc, owner);

    if(index == (size_t)(-1)) return MEM_CLAIM_CLAIM_LIMIT;

    size_t claims = desc->tracking.claimers[index].n_claims;
    if(claims + times < claims) return MEM_CLAIM_OVERFLOW;

    return CHECK_PASS;
}

static int visit_request_check(vpage_range_desc_t *desc, size_t base, __unused mop_internal_t* owner, size_t length) {
    if(desc->allocation_type != open_node) {
        // Can only request open nodes. This is not an error however, if we are searching.
        if(base == 0 && desc->start + desc->length != MAX_VIRTUAL_PAGES) return VISIT_CONT;
        else return MEM_REQUEST_NONE_FOUND;
    }

    // Has to be long enough to request the entire thing
    if(desc->length - (base - desc->start) < length) return VISIT_CONT;

    return CHECK_PASS;
}

static int visit_free_check(vpage_range_desc_t *desc, __unused size_t base, mop_internal_t* owner, __unused size_t length, __unused size_t times) {
    if(desc->allocation_type != allocation_node) {
        // Won't be claimed anyway
        return VISIT_CONT;
    }

    size_t index = find_free_claim_index(desc, owner);

    // Does not have a claim on this range
    if(index == (size_t)(-1) || desc->tracking.claimers[index].owner != owner) {
        printf(KRED"WARNING Got a free for something unclaimed!\n"KRST);
        mmap_dump_desc(desc);
        return VISIT_CONT;
    }

    return CHECK_PASS;
}

static vpage_range_desc_t *
visit_claim(vpage_range_desc_t *desc, __unused size_t base, mop_internal_t* owner, __unused size_t length, size_t times) {

    size_t index = find_free_claim_index(desc, owner);

    if(desc->tracking.claimers[index].owner == owner) {
        desc->tracking.claimers[index].n_claims += times;
    } else {
        claim_node(owner, desc, index, times);
    }

    return desc;
}

/* We should be careful not to free a table that contains an address we are rovking or the nano kernel will complain */
static vpage_range_desc_t* desc_being_revoked;

/* Tries to merge tombstone nodes */

static vpage_range_desc_t* pull_up_node(vpage_range_desc_t* child) {
    // We can pull this table into the parent
    assert(child->allocation_type != internal_node);
    vpage_range_desc_t* parent = hard_index_parent(child->start, child);


    if(parent == NULL || parent->start != child->start || parent->length != child->length) return child;

    assert(parent->allocation_type == internal_node);

    struct vpage_range_desc_table_t* parents_table = parent->sub_table;

    memcpy(parent, child, sizeof(vpage_range_desc_t));

    // The allocation pointers now need fixing. They point out correctly but do not point in correctly due to the move
    if(parent->allocation_type == allocation_node) {
        fixup_claims(parent);
    } else if(parent->allocation_type == open_node || parent->allocation_type == tomb_node) {
        fixup_desc_chain(parent);
    }

    child->allocation_type = free_node;
    child->length = 0;

    desc_table_free(parents_table);

    return pull_up_node(parent);
}

static void downward_correct_end(size_t page_n, size_t new_end) {

    vpage_range_desc_t* l = & desc_table_root.descs[L0_INDEX(page_n << UNTRANSLATED_BITS)];
    if(l->allocation_type != internal_node) {
        return;
    }
    size_t old_end = l->start + l->length;
    if(new_end > old_end) {
        l->length += new_end-old_end;
        assert(l->length != 0xFFFF7E8);
    }

    l = & l->sub_table->descs[L1_INDEX(page_n << UNTRANSLATED_BITS)];
    if(l->allocation_type != internal_node) {
        return;
    }
    old_end = l->start + l->length;
    if(new_end > old_end) {
        l->length += new_end-old_end;
        assert(l->length != 0xFFFF7E8);
    }
}

static void downward_correct_start(size_t page_n, size_t new_start) {
    size_t adjust = new_start - page_n;
    vpage_range_desc_t* l = & desc_table_root.descs[L0_INDEX(page_n << UNTRANSLATED_BITS)];
    if(l->allocation_type != internal_node) {
        return;
    }
    if(l->start == page_n) {
        l->start = new_start;
        l->length -=adjust;
    }

    l = & l->sub_table->descs[L1_INDEX(page_n << UNTRANSLATED_BITS)];
    if(l->allocation_type != internal_node) {
        return;
    }
    if(l->start == page_n) {
        l->start = new_start;
        l->length -=adjust;
    }

}

/* Tries to merge left with right (next comes after right). Returmns the rightmost out of left and right after merging */
static vpage_range_desc_t * merge_index(vpage_range_desc_t *left_desc, vpage_range_desc_t *right_desc,
                                        vpage_range_desc_t *next_desc) {

    if(left_desc == desc_being_revoked || right_desc == desc_being_revoked) return right_desc;

    // TODO: We may wish to merge with open nodes if it would reduce fragmentation.

    if(left_desc->allocation_type == tomb_node && right_desc->allocation_type == tomb_node) {


        int left_alloc_node = left_desc->allocated_length != 0;
        int right_alloc_node = right_desc->allocated_length != 0;


        if(! right_alloc_node || left_alloc_node) {
            // Can't merge right alloc node on to left partial node

            size_t transfer_amount = right_desc->length;
            size_t left_amount = left_desc->length;

            left_desc->length += transfer_amount;

            left_desc->allocated_length += right_desc->allocated_length;

            if(next_desc != NULL) {
                next_desc->prev = left_desc->start;
            }

            remove_from_pool(right_desc);

            right_desc->length = 0;
            right_desc->allocation_type = free_node;

            if(right_alloc_node) {
                // We are merging two reservation nodes
                // Merge reservation nodes, and unmap page containing right
                rescap_merge(left_desc->reservation, right_desc->reservation);
            }

            if(left_amount != 1) vmem_free_single((right_desc->start-1) << UNTRANSLATED_BITS);
            if(transfer_amount != 1) vmem_free_single(right_desc->start << UNTRANSLATED_BITS);

            /* Our parent nodes may need start and end adjusting */
            downward_correct_end(left_desc->start, left_desc->start + left_desc->length);
            downward_correct_start(right_desc->start, right_desc->start + transfer_amount);

            /* If there is only one node in a table we pull it up */
            if(next_desc != NULL && next_desc != desc_being_revoked) pull_up_node(next_desc);
            return pull_up_node(left_desc);
        }
    }

    return right_desc;
}

// Collect at least a few L1s worth
// define MIN_REVOKE (PAGE_TABLE_ENT_PER_TABLE*4)

static void revoke_sanity(__unused capability arg, __unused ptable_t table, readable_table_t* RO, size_t index, __unused size_t rep_pages) {
    register_t state = RO->entries[index];

    assert_int_ex(state, == , VTABLE_ENTRY_USED);
}

void revoke_start(vpage_range_desc_t* desc) {
    assert(desc->allocation_type == tomb_node);
    assert(desc->allocated_length == desc->length);

    desc_being_revoked = desc;

    res_t  res = desc->reservation;

    assert(res != NULL);

    // revoke a res
    size_t base = desc->start << UNTRANSLATED_BITS;
    size_t len = desc->length << UNTRANSLATED_BITS;

    res_nfo_t nfo = rescap_nfo(res);
    size_t base_r = (nfo.base - RES_META_SIZE);
    size_t len_r = (nfo.length + RES_META_SIZE);

    printf("Revoke: Revoking from %lx to %lx (%lx pages)\n", base, base+len, desc->length);

    assert_int_ex(base_r, ==, base);
    assert_int_ex(len_r, ==, len);

    rescap_revoke_start(res); // reads info from the reservation;

    // Now we have finished reading metadata we can free the pages

    vmem_free_single(base);
    vmem_free_single((base+len)-1);

    // Check everything is unmapped
    if(REVOKE_SANITY) {
        printf("Sanity Check %lx to %lx\n", base, base+len);
        vmem_visit_range(desc->start << UNTRANSLATED_BITS, desc->length, revoke_sanity, NULL);
    }
}

// Argument is a hint that there is something new
static inline void find_something_to_revoke(vpage_range_desc_t *desc) {
    if(desc_being_revoked == NULL) {
        vpage_range_desc_t * to_revoke = NULL;
        if(desc) {
            if(desc->allocated_length >= MIN_REVOKE && desc->length == desc->allocated_length) to_revoke = desc;
        } else {
            size_t max = MIN_REVOKE-1;
            FOREACH_IN_POOL(search_desc, PAGE_POOL_TOMB) {
                if(search_desc->allocated_length >= max && search_desc->length == search_desc->allocated_length) {
                    to_revoke = search_desc;
                    max = search_desc->allocated_length;
                }
            }
        }
        if(to_revoke) {
            remove_from_pool(to_revoke);
            revoke_start(to_revoke);
            printf("Sending revoke request...\n");
            revoke();
        }
    }
}

static vpage_range_desc_t * free_desc(vpage_range_desc_t *desc) {
    // 1: Unmap all pages (but NOT the first or last page)

    size_t free_start = desc->start;
    size_t free_len = desc->length;

    // We maintain the last/first page of every range as sentinel.
    // The first is needed to hold reservation metadata
    // The last is needed to stop freeing of tables
    if(free_len >= 3) {
        vmem_free_range((free_start + 1)  << UNTRANSLATED_BITS, free_len-2);
    }

    // 2: mark as tomb

    desc->allocation_type = tomb_node;
    add_to_pool(desc, PAGE_POOL_TOMB);

    // 3 : merge with previous

    vpage_range_desc_t *prev = hard_index(desc->prev);
    vpage_range_desc_t *next = hard_index(desc->start + desc->length);

    desc = merge_index(prev, desc, next);

    // 5: merge with next
    size_t next_ndx = (desc->start + desc->length);
    next = hard_index(next_ndx);

    if(next != NULL) {

        vpage_range_desc_t * next_next = hard_index(next->start + next->length);
        vpage_range_desc_t * mergeres = merge_index(desc, next, next_next);


        if(mergeres != next) {
            desc = mergeres;
        }
    }

    if(desc_being_revoked == NULL) find_something_to_revoke(desc);

    return desc;
}

static vpage_range_desc_t *
visit_free(vpage_range_desc_t *desc, __unused size_t base, mop_internal_t* owner, __unused size_t length, size_t times) {

    size_t index = find_free_claim_index(desc, owner);
    size_t rem_claims =  desc->tracking.claimers[index].n_claims;

    rem_claims = rem_claims <= times ? 0 : rem_claims - times;
    desc->tracking.claimers[index].n_claims = rem_claims;

    if(rem_claims == 0) {
        release_claim(desc, index);
        if(desc->claims_used == 0) {
            desc = free_desc(desc);
        }
    }

    return desc;
}

static vpage_range_desc_t* leaf_to_internal(vpage_range_desc_t* desc, size_t transfer_to_index) {
    assert(desc->allocation_type != internal_node);

    vpage_range_desc_table_t *tbl = desc_table_alloc();

    vpage_range_desc_t* sub_desc = & tbl->descs[transfer_to_index];

    if(desc == desc_being_revoked) {
        desc_being_revoked = sub_desc;
    }

    tbl->ranges_allocated = 1;
    tbl->pages_allocated = desc->length;

    memcpy(sub_desc, desc, sizeof(vpage_range_desc_t));
    desc->sub_table = tbl;
    desc->reservation = NULL;
    desc->allocated_length = 0;
    desc->allocation_type = internal_node;

    // The allocation pointers now need fixing. They point out correctly but do not point in correctly due to the move
    if(sub_desc->allocation_type == allocation_node) {
        fixup_claims(sub_desc);
    } else if(sub_desc->allocation_type == open_node || sub_desc->allocation_type == tomb_node) {
        if(sub_desc->allocation_type == open_node) assert(sub_desc->reservation != NULL);
        fixup_desc_chain(sub_desc);
    }

    bzero(&desc->tracking, sizeof(desc->tracking));

    return sub_desc;
}

struct split_res {
    res_t reservation;
    enum allocation_type_t type;
    vpage_range_desc_t* transfer_from;
};

static void push_in_index(size_t page_n, size_t prev, size_t length,
                          vpage_range_desc_table_t *tbl, size_t lvl,
                          struct split_res* split_result, vpage_range_desc_t** desc_out) {
    size_t ndx = NDX(page_n, lvl);
    vpage_range_desc_t* desc = & tbl->descs[ndx];

    assert(lvl <= 2);

    if(desc->length == 0) {
        // Space for a leaf here
        desc->start = page_n;
        desc->prev = prev;
        desc->length = length;

        desc->reservation = split_result->reservation;
        desc->allocation_type = split_result->type;
        desc->allocated_length = 0;

        if(desc->allocation_type == allocation_node) {
            transfer_claims(split_result->transfer_from, desc);
        }
#ifdef MAX_POOLS
        else if(desc->allocation_type == open_node || desc->allocation_type == tomb_node) {
            if(desc->allocation_type == open_node) assert(desc->reservation != NULL);
            if(split_result->transfer_from->tracking.free_chain.pool_id) add_after_desc(split_result->transfer_from, desc);
        }
#endif
        *desc_out = desc;

    } else {
        // Have to recurse again.
        size_t new_length = desc->start + desc->length - page_n;
        size_t new_start = page_n;

        if(desc->allocation_type != internal_node) {
            leaf_to_internal(desc, NDX(desc->start, lvl+1));
        }

        push_in_index(page_n, prev, length, desc->sub_table, lvl+1, split_result, desc_out);

        desc->start = new_start;
        desc->length = new_length;
    }
}

static void split_index(size_t page_n, struct index_result* containing_index,
                                    vpage_range_desc_table_t *tbl,
                                    size_t lvl,
                                    struct split_res* split_result,
                                    vpage_range_desc_t** desc_out,
                                    int out_of_parent) {
    assert(lvl <= 2);

    size_t left = containing_index->indexs[lvl];
    size_t middle = NDX(page_n, lvl);

    vpage_range_desc_t* left_desc = & tbl->descs[left];

    size_t length1 = page_n - left_desc->start;
    size_t length2 = left_desc->length - length1;

    assert(length1 != 0);
    assert(length2 != 0);

    if(out_of_parent || left != middle) {
        // Left node no longer summarises as much.
        size_t right_pagen = left_desc->start + left_desc->length;

        left_desc->length = length1;
        split_result->type = left_desc->allocation_type;
        split_result->transfer_from = left_desc;

        if(left_desc->allocation_type == allocation_node) {

        } else if(left_desc->allocation_type == open_node) {
            split_result->reservation =
                    rescap_split(left_desc->reservation, (left_desc->length << UNTRANSLATED_BITS) - RES_META_SIZE);
            assert(split_result->reservation != NULL);
        } else if(left_desc->allocation_type == internal_node) {
            split_index(page_n, containing_index, left_desc->sub_table, lvl+1, split_result, desc_out, 1);
        } else {
            assert(0 && "For some reason we are trying to split a tomb or invalid node");
        }

        if(!out_of_parent) {

            if(right_pagen != MAX_VIRTUAL_PAGES) {
                vpage_range_desc_t* right_desc = hard_index(right_pagen);
                right_desc->prev = page_n;
            }

            push_in_index(page_n, split_result->transfer_from->start, length2, tbl, lvl, split_result, desc_out);
        }

    } else {
        // We just need to recurse into left.
        if(left_desc->allocation_type != internal_node) {
            size_t new_ndx = NDX(left_desc->start, lvl+1);

            containing_index->result = leaf_to_internal(left_desc, new_ndx);
            containing_index->indexs[lvl+1] = new_ndx;
        }

        split_index(page_n, containing_index, left_desc->sub_table, lvl+1, split_result, desc_out, 0);
    }

}

mop_internal_t first_mop;

mop_t __init_mop(capability sealing_cap, res_t big_res) {
    static int once = 0;

    if(once == 0) {
        once = 1;
        mop_sealing_cap = sealing_cap;
        init_desc(big_res);
        bzero(&first_mop, sizeof(mop_internal_t));
        bzero(&mmap_mop, sizeof(mop_internal_t));
        first_mop.debug_id = "init_mop";
        return seal_mop(&first_mop);
    } else return NULL;
}

#define ALIGN_PAGE_REQUEST(base, length, page_n, npages)    \
    size_t page_n = base >> UNTRANSLATED_BITS;              \
    size_t npages = (align_up_to((base + length), UNTRANSLATED_PAGE_SIZE) >> UNTRANSLATED_BITS) - page_n;


void size_node(size_t page_n, size_t npages, struct index_result* index) {
    struct split_res splitRes;
    vpage_range_desc_t* desc;

    if(index->result->start < page_n) {
        // Split off the first part of for correct start
        split_index(page_n, index, & desc_table_root, 0, &splitRes, &desc, 0);
        index->result = desc;
        index->indexs[0] = NDX(page_n, 0);
        index->indexs[1] = NDX(page_n, 1);
        index->indexs[2] = NDX(page_n, 2);
    }

    if(index->result->length > npages) {
        // Split off the end to make correct length
        size_t req_end = page_n + npages;
        split_index(req_end, index, & desc_table_root, 0, &splitRes, &desc, 0);
    }
}

typedef vpage_range_desc_t * visit_t(vpage_range_desc_t *desc, size_t page_n, mop_internal_t* owner, size_t npages, size_t times);
typedef int check_t(vpage_range_desc_t *desc, size_t page_n, mop_internal_t* owner, size_t npages, size_t times);

void mmap_dump(void) {
    size_t search_page_n = 0;
    do {

        vpage_range_desc_t* desc = hard_index(search_page_n);

        mmap_dump_desc(desc);

        assert(desc->length != 0);

        search_page_n += desc->length;

    } while(search_page_n != MAX_VIRTUAL_PAGES);
}

typedef struct {
    size_t phy_consumed;
    size_t skipped;
} count_t;

static void visit_count_maps(capability arg, __unused ptable_t table, readable_table_t* RO, size_t index, size_t rep_pages) {
    count_t* total = (count_t*)arg;

    table_entry_t entry = RO->entries[index];
    if(rep_pages == 1 && entry != VTABLE_ENTRY_USED && entry != VTABLE_ENTRY_FREE && entry != VTABLE_ENTRY_TRAN) total->phy_consumed+=2;
    if(entry == VTABLE_ENTRY_TRAN) total->skipped += (2 * rep_pages);
}

static void dump_mop(mop_internal_t* mop, int do_children) {
    printf("Mop: %s (%d)", mop->debug_id, mop->state);
    size_t actual_alloc = 0;
    size_t actual_ranges = 0;
    size_t unique_ranges = 0;
    size_t unique_alloc = 0;
    size_t phy_alloc = 0;
    size_t phy_alloc_unique = 0;
    size_t skipped = 0;
    FOREACH_CLAIMED_RANGE(mop, desc, link) {
        int unique = 1;
        FOREACH_CLAIMER(desc, ndx, clm) {
            if(clm->owner != NULL && clm->owner != mop) {
                unique = 0;
                break;
            }
        }

        count_t count;
        count.skipped = 0;
        count.phy_consumed = 0;

        vmem_visit_range(desc->start, desc->length, &visit_count_maps, &count);

        actual_ranges++;
        phy_alloc+= count.phy_consumed;
        actual_alloc +=desc->length;
        skipped +=count.skipped;

        if(unique) {
            unique_alloc +=desc->length;
            unique_ranges ++;
            phy_alloc_unique += count.phy_consumed;
        }
    }

    printf("|-- Ranges %lx (actual = %lx, unique = %lx). Pages %lx (actual = %lx, unique = %lx). Phy Pages actual = %lx, unique = %lx. Skipped = %lx\n",
            mop->allocated_ranges, actual_ranges, unique_ranges, mop->allocated_pages, actual_alloc, unique_alloc, phy_alloc, phy_alloc_unique, skipped);

    for(mop_internal_t* child = mop->child_mop; child != NULL; child = child->next_sibling_mop) {
        printf("|-- Parent of %s (%d)\n", child->debug_id, child->state);
    }

    if(do_children) {
        for(mop_internal_t* child = mop->child_mop; child != NULL; child = child->next_sibling_mop) {
            dump_mop(child, do_children);
        }
    }

}

static void check_mapping_leak(void) {
    // First loop throgh book
    size_t phy_map = 0;

    printf("Counting phy...\n");
    for(size_t i = 0; i != BOOK_END; i += book[i].len) {
        if(book[i].status == page_mapped) phy_map += book[i].len;
    }

    printf("Counting virt...\n");
    count_t virt_map;
    virt_map.skipped = 0;
    virt_map.phy_consumed = 0;

    vmem_visit_range(0, (size_t)(MAX_VIRTUAL_PAGES), visit_count_maps, (capability)&virt_map);

    printf("There are %lx physical pages used in mappings. Virtual pages are mapping to %lx pages (%lx skipped)\n", phy_map, virt_map.phy_consumed, virt_map.skipped);
}


void full_dump(void) {
    pmem_print_book(book, 0, -1);
    check_mapping_leak();
    dump_mop(&first_mop, 1);
    mmap_dump();
}

static int mem_claim_or_release(size_t base, size_t length, size_t times, mop_internal_t* mop, visit_t* visitf, check_t* checkf) {
    ALIGN_PAGE_REQUEST(base, length, page_n, npages)

    if(npages == 0) return 0;
    if(page_n == 0) return MEM_BAD_BASE;

    struct index_result index;

    soft_index(page_n, &index);


    do {
        int check = checkf(index.result, page_n, mop, npages, times);
        if(check <= VISIT_DONE) return check;

        if(check == CHECK_PASS) {
            size_node(page_n, npages, &index);
            index.result = visitf(index.result, page_n, mop, npages, times);
        }

        size_t result_ends_at = index.result->start + index.result->length;

        if(result_ends_at >= page_n + npages) {
            break;
        }

        npages -= (result_ends_at - page_n);
        page_n = result_ends_at;

        index.indexs[0] = NDX(page_n, 0);
        index.indexs[1] = NDX(page_n, 1);
        index.indexs[2] = NDX(page_n, 2);
        index.result = hard_index(page_n);
    } while(1);

    return MEM_OK;
}

static int claim_mop(mop_internal_t* mop) {
    return mem_claim_or_release(cheri_getbase(mop),sizeof(mop_internal_t), 1, &mmap_mop, &visit_claim, &visit_claim_check);
}
static void release_mop(mop_internal_t* mop) {
    mem_claim_or_release(cheri_getbase(mop),sizeof(mop_internal_t), 1, &mmap_mop, &visit_free, &visit_free_check);
}

static void adjust_desc_pool(vpage_range_desc_t* desc) {
    if(desc) {
        assert(desc->allocation_type == open_node);
        if(((desc->tracking.free_chain.pool_id-1) == PAGE_POOL_FREE_LARGE) &&
            (desc->length < (POOL_LARGE_THRESHOLD >> UNTRANSLATED_BITS))) {
            remove_from_pool(desc);
            add_to_pool(desc, PAGE_POOL_FREE_SMALL);
        }
    }
}

ERROR_T(res_t) __mem_request(size_t base, size_t length, mem_request_flags flags, mop_t mop_sealed, size_t* phy_base) {
    mop_internal_t* mop = unseal_mop(mop_sealed);

    if(mop == NULL) return MAKE_ER(res_t, MEM_BAD_MOP);

    if(base & (RES_META_SIZE-1)) {
        printf("Base badly aligned\n");
        return MAKE_ER(res_t, MEM_BAD_BASE);
    }

    size_t align_power = 0;

    size_t req_base = base;
    size_t req_length = length;

    // Size for a parent node, and a child node to pass on to caller

    if(req_base != 0) base -= (2*RES_META_SIZE);
    length += (2*RES_META_SIZE);

    ALIGN_PAGE_REQUEST(base, length, page_n, npages)

    if(flags & ALIGN_TOP) {
        // We want all npages to have the same top bits we round up to a power of 2, and subtract 1 to get
        // the mask
        align_power = round_up_to_nearest_power_2(npages);
    }

    if(npages == 0) npages = 1;

    res_t result;
    struct index_result index;

    // We can always commit for dma for 1 page.
    if((flags & COMMIT_DMA) && npages == 1) {
        flags &= ~COMMIT_DMA;
        flags |= COMMIT_NOW;
    }

    // We require an extra page as the first can't be in the contiguous chunk as it is already committed
    // We could probably fix this by splitting the reservation later
    if(flags & COMMIT_DMA) npages++;

    if(req_base != 0) {
        soft_index(page_n, &index);

        int check = visit_request_check(index.result, page_n, mop, npages);

        if(check == VISIT_CONT) check = MEM_REQUEST_UNAVAILABLE;

        if (check <= VISIT_DONE) {
            printf("Mem request error: %d", check);
            return MAKE_ER(res_t, check);
        }

    } else {
        // Search. // TODO we may wish to allocate better than "first fit". Probably better to have many pools.

        size_t search_page_n = 0;
#ifdef MAX_POOLS
        size_t pool = (npages >= (POOL_LARGE_THRESHOLD >> UNTRANSLATED_BITS)) ?  PAGE_POOL_FREE_LARGE : PAGE_POOL_FREE_SMALL;
        vpage_range_desc_t* desc = peek_from_pool_head(pool);
        if(desc == NULL && pool == PAGE_POOL_FREE_SMALL) desc = peek_from_pool_head(PAGE_POOL_FREE_LARGE);
#endif
        while(1) {

#ifdef MAX_POOLS
            if(desc == NULL) {
                mmap_dump();
            }
            assert(desc != NULL);
            assert(desc->allocation_type == open_node);
            search_page_n = desc->start;
#else
            vpage_range_desc_t* desc = hard_index(search_page_n);
#endif


            if(desc->length == 0) {
                mmap_dump_desc(desc);
            }

            assert(desc->length != 0);

            if(flags & COMMIT_DMA) search_page_n++;

            if(desc->allocation_type == open_node && desc->length >= npages) {
                int can_use_range = !(flags & ALIGN_TOP) || ((search_page_n & ((align_power - 1))) + npages <= align_power);
                int can_use_part_range = (flags & ALIGN_TOP) && (desc->length >= 2 * npages);

                if(can_use_range) {
                    // Pretend like they requested the start
                    page_n = search_page_n;
                } else if(can_use_part_range) {
                    // This range is badly aligned, but long enough to give the alignment requested
                    page_n = align_up_to(search_page_n, align_power);
                }

                if(flags & COMMIT_DMA) search_page_n--;
                if(can_use_range || can_use_part_range) {
                    index.indexs[0] = NDX(search_page_n,0);
                    index.indexs[1] = NDX(search_page_n,1);
                    index.indexs[2] = NDX(search_page_n,2);
                    index.result = desc;
                    break;
                }

            }

#ifdef MAX_POOLS
            vpage_range_desc_t* next = desc->tracking.free_chain.next;
            desc = ((next == NULL) && (desc->tracking.free_chain.pool_id-1 == PAGE_POOL_FREE_SMALL)) ?
                        peek_from_pool_head(PAGE_POOL_FREE_LARGE) : next;
            if(desc == NULL)
#else
            search_page_n += desc->length;

            if(search_page_n == MAX_VIRTUAL_PAGES)
#endif
            {
                printf("Request from %s", mop->debug_id);
                printf(KRED"Failed to find a VMEM allocation base %lx. length %lx. flags %x\n"KRST, base, length, flags);
                panic("Search failed\n");
                return MAKE_ER(res_t, MEM_REQUEST_NONE_FOUND); // No pages matched
            }
        }

    }

    // Page skip accounted for
    if(flags & COMMIT_DMA) npages--;

    if((flags & (COMMIT_DMA | COMMIT_NOW))) {
        // must commit before we size node as sizing the node needs access to the reservation
        // TODO think of a good default for how pages to allocate contiguously for commit now
        if(npages > 1) {
            size_t commit_off = (flags & COMMIT_DMA) ? 0 : 1;
            size_t contig_base = vmem_commit_vmem_range((page_n+commit_off) << UNTRANSLATED_BITS, npages - commit_off, flags);
            if (phy_base) *phy_base = contig_base;
        } else if(phy_base) {
            *phy_base = translate_address(page_n << UNTRANSLATED_BITS, 1) + RES_META_SIZE;
        }
    }

    // This is where the reservation nodes are actually split
    size_node(page_n, npages, &index);

    if(flags & COMMIT_DMA && npages > 1) {
        // We skipped a page. If there is exactly one before this allocation, kill it to stop fragmentation
        vpage_range_desc_t* single_desc  = hard_index(page_n-1);

        if(single_desc && single_desc->length == 1 && single_desc->allocation_type == open_node) {
            single_desc->allocated_length = 1;
            remove_from_pool(single_desc);
            free_desc(single_desc);
        }
    }

#ifdef MAX_POOLS
    vpage_range_desc_t* prev = index.result->tracking.free_chain.prev;
    vpage_range_desc_t* next = index.result->tracking.free_chain.next;
    remove_from_pool(index.result);
    // These now might be too small to qualify as large
    adjust_desc_pool(prev);
    adjust_desc_pool(next);
#endif

    result = index.result->reservation;
    index.result->allocated_length = index.result->length;

    assert(index.result->allocation_type == open_node);

    index.result->allocation_type = allocation_node;

    for(size_t i = 0; i != MAX_CLAIMERS; i++) {
        index.result->tracking.claimers[i].owner = NULL;
    }

    visit_claim(index.result, base, mop, length, 1);

    assert(result != NULL);

    result = rescap_parent(result);
    assert(result != NULL);

    base = (page_n << UNTRANSLATED_BITS) + (2 * RES_META_SIZE);
    length = (npages << UNTRANSLATED_BITS) - (2 * RES_META_SIZE);

    // Only trim the reservation if a specific range was requested. Otherwise we are happy to give extra so more alignment can happen
    if(req_base) {
        if(base < req_base) {
            size_t bytes_too_many = req_base-base;
            result = rescap_split(result, bytes_too_many-RES_META_SIZE);
            assert(result != NULL);
            base += bytes_too_many;
            length -= bytes_too_many;

            if(length > req_length) {
                rescap_split(result, length);
                assert(result != NULL);
            }
        }
    } else if (flags & EXACT_SIZE) {
        if(length > req_length) {
            size_t bytes_too_many = length-req_length;
            result = rescap_split(result, bytes_too_many-RES_META_SIZE);
        }
    }

    return MAKE_VALID(res_t, result);
}

int reclaim(mop_internal_t* mop, int remove_from_chain) {
    mop->state = reclaiming;

    /* First reclaim all children */
    mop_internal_t* child = mop->child_mop;

    while(child != NULL) {

        /* Do not move this after reclaim. Reclaim will make the child invalid */
        mop_internal_t* next_child = child->next_sibling_mop;

        reclaim(child, 0);

        // TODO: Once we have resource limits, we would recover them at this point

        child = next_child;
    }

    /* Then release all claims */
    restart_loop:{}

    FOREACH_CLAIMED_RANGE(mop, desc, lnk) {
        // A visit_free might invalidate a chain if the next node is pulled up. Rather than track whether this happens
        // we instead just notice the node is apparently free. Short of another error, this probably means it was pulled up.
        // Restarting will work just fine as a pull up will fix the head ptr
        if(desc->allocation_type == free_node) goto restart_loop;
        assert(mop == claim->owner);
        assert_int_ex(desc->allocation_type, ==, allocation_node);
        claim->n_claims = 1;
        visit_free(desc, desc->start, mop, desc->length, 1);
    }

    // Should have removed every claim
    assert(LINK_IS_END(mop->first));

    /* Remove from sibling chain */
    if(remove_from_chain) {
        mop_internal_t* prev = mop->prev_sibling_mop;
        mop_internal_t* next = mop->next_sibling_mop;

        if(next != NULL) {
            next->prev_sibling_mop = prev;
        }
        if(prev != NULL) {
            prev->next_sibling_mop = next;
        } else {
            mop->parent_mop->child_mop = next;
        }
    }

    mop->state = dead;

    /* Then release our claim on the mop. WE MUST NOT ACCESS IT AFTER THIS */

    release_mop(mop);

    return 0;
}

int __mem_reclaim_mop(mop_t mop_sealed) {
    mop_internal_t* mop = unseal_mop(mop_sealed);

    if(mop == NULL) return MEM_BAD_MOP;

    reclaim(mop, 1);

    return MEM_OK;
}

// FIXME should we allocate this space ourselves? probably.

ERROR_T(mop_t) __mem_makemop(res_t space, mop_t mop_sealed, const char* debug_id) {
    mop_internal_t* mop = unseal_mop(mop_sealed);

    if(mop == NULL) return MAKE_ER(mop_t, MEM_BAD_MOP);

    cap_pair pair;
    try_take_res(space, sizeof(mop_internal_t), &pair);

    mop_internal_t* new_mop = (mop_internal_t*)pair.data;

    if(new_mop == NULL) return MAKE_ER(mop_t, MEM_MAKEMOP_BAD_SPACE);

    if(claim_mop(new_mop) != MEM_OK) return MAKE_ER(mop_t, MEM_BAD_MOP_CANT_CLAIM);

    bzero(new_mop, sizeof(mop_internal_t));

    new_mop->parent_mop = mop;
    new_mop->debug_id = debug_id;

    if(mop->child_mop != NULL) {
        new_mop->next_sibling_mop = mop->child_mop;
        mop->child_mop->prev_sibling_mop = new_mop;
    }

    mop->child_mop = new_mop;

    return MAKE_VALID(mop_t,seal_mop(new_mop));

}

int __mem_claim(size_t base, size_t length, size_t times, mop_t mop_sealed) {
    mop_internal_t* mop = unseal_mop(mop_sealed);

    if(mop == NULL) return MEM_BAD_MOP;

    return mem_claim_or_release(base, length, times, mop, &visit_claim, &visit_claim_check);
}

int __mem_release(size_t base, size_t length, size_t times, mop_t mop_sealed) {
    mop_internal_t* mop = unseal_mop(mop_sealed);

    if(mop == NULL) return MEM_BAD_MOP;

    return mem_claim_or_release(base, length, times, mop, &visit_free, &visit_free_check);
}

void __revoke_finish(res_t res) {

    vpage_range_desc_t* desc = desc_being_revoked;

    res_nfo_t nfo = rescap_nfo(res);

    printf("Got reservation back %lx to %lx\n", nfo.base, nfo.base + nfo.length);

    size_t base = (nfo.base - RES_META_SIZE) >> UNTRANSLATED_BITS;
    size_t len = (nfo.length + RES_META_SIZE) >> UNTRANSLATED_BITS;

    assert_int_ex(base, ==, desc->start);
    assert_int_ex(len, ==, desc->length);

    desc->reservation = res;
    desc->allocation_type = open_node;

    set_not_in_pool(desc);

    desc_being_revoked = NULL;

    vpage_range_desc_t* now_free = pull_up_node(desc);

    assert(now_free->reservation != NULL);
    add_to_pool(now_free, (desc->length >= (POOL_LARGE_THRESHOLD >> UNTRANSLATED_BITS)) ? PAGE_POOL_FREE_LARGE : PAGE_POOL_FREE_SMALL);

    printf("Revoke: finished!\n");

    find_something_to_revoke(NULL);
}

void __revoke(void) {

    assert(worker_id == 2);

    res_t  res = rescap_revoke_finish();

    if(!cheri_gettag(res)) {
        int er = cheri_getoffset(res);
        printf(KRED"Revoke error code %d\n", er);
    }
    assert(res != NULL);
    assert(cheri_gettag(res));

    printf("Revoke: Sending revoke finish back to main thread\n");

    revoke_finish(res);
}