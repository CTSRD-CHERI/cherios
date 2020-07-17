/*-
 * Copyright (c) 2020 Lawrence Esswood
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

#include "cheric.h"
#include "nano/nanokernel.h"
#include "mman.h"
#include "lists.h"
#include "stdlib.h"
#include "atomic.h"

/* This version of capmalloc only uses slabs for allocation.
 * Each tread has its own set of slabs.
 *
 * Claim tracking is concurrent and for the entire process.
 * If there is too much pressure on the cache (or NO_CACHE is 1) then
 * are forwarded directly to the memory manager
 */


// Claim tracking, all of this must be concurrent

 // Size of each slab, including headers and space required by MMAN
#define SLAB_SIZE_BITS (UNTRANSLATED_BITS + L2_BITS)
#define SLAB_SIZE   (1 << SLAB_SIZE_BITS)

#define NO_CACHE 0

// TODO: Currently freeing is very aggressive, we might wish to coalesce frees.
// TODO: If the cache gets big do some eviction. Otherwise it can grow in an unbounded way.
#define ALLOW_EVICTION 0

// We allocate blocks of this size for tracking metadata. Quite large so as not to waste too much due to fragmentation.
#define TRACKING_BLOCK_SIZE ((256 * UNTRANSLATED_PAGE_SIZE) - MEM_REQUEST_FAST_OFFSET)

#define IN_SAME_PAGE(X, Y) ((((size_t)(X)) >> UNTRANSLATED_BITS) == (((size_t)(Y)) >> UNTRANSLATED_BITS))

// ~2064 bytes
struct l2_table_t {
    l2_table_t* free_chain;
    // mman can do 64 bits of counting, but this can probably be smaller
    volatile uint32_t claim_counts[PAGE_TABLE_ENT_PER_TABLE];
};

// ~12304 bytes
// TODO might want faster super-page claim (rather than looping over every sub table)
struct l1_table_t {
    l1_table_t* free_chain;
    l2_table_t *volatile tables[PAGE_TABLE_ENT_PER_TABLE];
    // A count of the total number of claims in a subtable (useful for tracking eviction)
    volatile uint64_t total_claims[PAGE_TABLE_ENT_PER_TABLE];
};

struct l0_table_t {
    l1_table_t *volatile tables[PAGE_TABLE_ENT_PER_TABLE];
    volatile uint64_t total_claims[PAGE_TABLE_ENT_PER_TABLE];
};

res_nfo_t round_nfo_to_page(res_nfo_t nfo) {
    size_t mask = (UNTRANSLATED_PAGE_SIZE-1);
    // round base down
    size_t rounded_base = nfo.base &~ mask;
    size_t rounded_length = nfo.length + (nfo.base-rounded_base);
    // round length up
    rounded_length = (rounded_length + mask) &~mask;
    return MAKE_NFO(rounded_length, rounded_base);
}

#if (!NO_CACHE)

l0_table_t l0_table;
l1_table_t*volatile l1_free_chain;
l2_table_t*volatile l2_free_chain;

template<class T>
T* pop_from_free_chain(T*volatile* head) {
    T* ret;

    while((ret = *head)) {
        T* next = ret->free_chain;
        int success = ATOMIC_CAS_RV(head, c, ret, next);
        if(success) break;
    }

    return ret;
}

template<class T>
void push_to_free_chain(T* first, T*last, T*volatile* head) {
    int success;
    do {
        T* old_head = *head;
        last->free_chain = old_head;
        success = ATOMIC_CAS_RV(head, c, old_head, first);
    } while(!success);
}

__thread res_t tracking_res_chunk;
__thread size_t tracking_res_sz = 0;

void* new_tracking_object(size_t size) {
    if(size > tracking_res_sz) {
        tracking_res_chunk = mem_request(0, TRACKING_BLOCK_SIZE, NONE, own_mop).val;
        tracking_res_sz = TRACKING_BLOCK_SIZE;
    }

    res_t res = tracking_res_chunk;

    if(tracking_res_sz > size + RES_META_SIZE) {
        tracking_res_chunk = rescap_split(res, size);
        tracking_res_sz -= (size + RES_META_SIZE);
    } else {
        tracking_res_sz = 0;
    }

    _safe cap_pair pair;

    rescap_take(res, &pair);

    return pair.data;
}

template<class T>
T* allocate_obj(T*volatile* head) {
    T* ret = pop_from_free_chain<T>(head);

    if(!ret) {
        ret = (T*)new_tracking_object(sizeof(T));
    }

    return ret;
}

l2_table_t* allocate_l2_table(void) {
    return allocate_obj<l2_table_t>(&l2_free_chain);
}

l1_table_t* allocate_l1_table(void) {
    return allocate_obj<l1_table_t>(&l1_free_chain);
}

void deallocate_l2_table(l2_table_t* table) {
    push_to_free_chain<l2_table_t>(table, table, &l2_free_chain);
}

void deallocate_l1_table(l1_table_t* table) {
    push_to_free_chain<l1_table_t>(table, table, &l1_free_chain);
}

template<class T, T* (*AllocF)(), void (*FreeF)(T*)>
int try_add(T* volatile* add_to) {
    // Might get called concurrently so just abort if add_to ever becomes non-null

    T*  newTable = AllocF();

    int success = ATOMIC_CAS_RV(add_to, c, NULL, newTable);

    if(!success) {
        FreeF(newTable);
    }

    return success;
}


template<class PT, class CT, CT* (*AllocF)(), void (*FreeF)(CT*)>
CT* add_claims_parent_table(PT* table, size_t ndx, uint64_t total_to_add) {

    CT *volatile * childPtr = &table->tables[ndx];
    volatile uint64_t * total_ptr = &table->total_claims[ndx];

    int success;
    do {
        uint64_t total = *total_ptr;
        while(total == 0) {
            if(*childPtr == nullptr) {
                // Try to add our own table (gives us the right to bump the ctr)
                success = try_add<CT, AllocF, FreeF>(childPtr);

                if(success) {
                    (*total_ptr)=total_to_add;
                    goto super_break;
                }
            } else {
                // Table currently being removed / added by someone else
                sleep(0);
                total = *total_ptr;
            }
        }

        success = ATOMIC_CAS_RV(total_ptr, 64, total, total+total_to_add);
    } while(!success);

super_break:

    return *childPtr;
}

int add_claims_to_table(l0_table_t* table0, size_t base, size_t length, bool already_claimed) {

    size_t ndx_end = (base + length) >> UNTRANSLATED_BITS;
    size_t ndx = base >> UNTRANSLATED_BITS;

    bool first = true;

    size_t l0_mask = (1 << (L2_BITS+L1_BITS))-1;
    size_t l1_mask = (1 << (L2_BITS))-1;

    size_t prev_ndx = ndx;

    int res;

    l1_table_t* table1;
    l2_table_t* table2;

    while(ndx != ndx_end) {
        size_t remain = ndx_end-ndx;

        if(first || ((ndx & l0_mask) == 0)) {
            size_t n = (l0_mask + 1) - (ndx & l0_mask);

            if(n > remain) n = remain;

            table1 = add_claims_parent_table<l0_table_t,
                    l1_table_t,
                    allocate_l1_table,
                    deallocate_l1_table>(table0, ndx >> (L2_BITS+L1_BITS), n);
        }

        if(first || ((ndx & l1_mask) == 0)) {
            size_t n = (l1_mask + 1) - (ndx & l1_mask);

            if(n > remain) n = remain;

            table2 = add_claims_parent_table<l1_table_t,
                    l2_table_t,
                    allocate_l2_table,
                    deallocate_l2_table>(table1, (ndx >> L2_BITS) & ((1 << L1_BITS)-1), n);
        }

        uint32_t was_before = ATOMIC_ADD_RV((&table2->claim_counts[ndx & l1_mask]),32,64,1);

        if(!already_claimed && (was_before != 0)) {
            if(prev_ndx != ndx) {
                res = mem_claim(prev_ndx << UNTRANSLATED_BITS, (ndx-prev_ndx) << UNTRANSLATED_BITS, 1, own_mop);
                assert_int_ex(-res, ==, 0);
                if(res != 0) return res;
            }
            prev_ndx = ndx+1;
        }

        // Not too sure how you would achieve this without another bug
        assert(!(was_before != 0 && already_claimed) && "Probably claiming before allocating");

        ndx++;
        first = false;
    }

    res = 0;

    if(!already_claimed && prev_ndx != ndx) {
        res = mem_claim(prev_ndx << UNTRANSLATED_BITS, (ndx-prev_ndx) << UNTRANSLATED_BITS, 1, own_mop);
        assert_int_ex(-res, ==, 0);
    }

    return res;
}

template<class PT, class CT>
CT* remove_claims_from_parent(PT* table, size_t ndx, uint64_t total_to_remove) {

    CT* child = table->tables[ndx];
    volatile uint64_t * total_ptr = &table->total_claims[ndx];

    uint64_t old_val = ATOMIC_ADD_RV(total_ptr, 64, 64, -total_to_remove);

    // While we are caching everything it should not be possible to free something unless claimed
    assert(old_val != 0 && "Probably a double free");

    if(old_val == total_to_remove) table->tables[ndx] = nullptr;

    return (old_val == total_to_remove) ? child : nullptr;
}

int remove_claims_from_table(l0_table_t* table0, size_t base, size_t length) {

    size_t ndx_end = (base + length) >> UNTRANSLATED_BITS;
    size_t ndx = base >> UNTRANSLATED_BITS;

    bool first = true;

    size_t l0_mask = (1 << (L2_BITS + L1_BITS)) - 1;
    size_t l1_mask = (1 << (L2_BITS)) - 1;

    l1_table_t* to_free_1 = nullptr;
    l2_table_t* to_free_2 = nullptr;

    size_t prev_ndx = ndx;

    int res;

    l1_table_t* table1;
    l2_table_t* table2;

    while (ndx != ndx_end) {
        size_t remain = ndx_end-ndx;

        if(first || ((ndx & l0_mask) == 0)) {

            if(to_free_1) deallocate_l1_table(to_free_1);

            size_t n = (l0_mask + 1) - (ndx & l0_mask);

            if(n > remain) n = remain;

            size_t sub_ndx = ndx >> (L2_BITS+L1_BITS);

            table1 = table0->tables[sub_ndx];

            to_free_1 = remove_claims_from_parent<l0_table_t, l1_table_t>(table0,
                                                                          sub_ndx, n);
        }

        if(first || ((ndx & l1_mask) == 0)) {

            if(to_free_2) deallocate_l2_table(to_free_2);

            size_t n = (l1_mask + 1) - (ndx & l1_mask);

            if(n > remain) n = remain;

            size_t sub_ndx = (ndx >> L2_BITS) & ((1 << L1_BITS)-1);

            table2 = table1->tables[sub_ndx];

            to_free_2 = remove_claims_from_parent<l1_table_t, l2_table_t>(table1,
                    sub_ndx, n);
        }


        uint32_t was_before = ATOMIC_ADD_RV(&table2->claim_counts[ndx & l1_mask],32,64,-1);

        assert(was_before != 0 && "Almost certainly a double free");

        if(was_before != 1) {
            if(ndx != prev_ndx) {
                res = mem_release(prev_ndx << UNTRANSLATED_BITS, (ndx-prev_ndx) << UNTRANSLATED_BITS, 1, own_mop);
                assert(res == 0);
                if(res != 0) return res;
            }
            prev_ndx = ndx+1;
        }

        ndx++;
        first = false;
    }

    res = 0;

    if(ndx != prev_ndx) {
        res = mem_release(prev_ndx << UNTRANSLATED_BITS, (ndx-prev_ndx) << UNTRANSLATED_BITS, 1, own_mop);
        assert(res == 0);
        if(res != 0) return res;
    }

    if(to_free_1) deallocate_l1_table(to_free_1);
    if(to_free_2) deallocate_l2_table(to_free_2);

    return res;
}

#endif // (NO_CACHE)

int claim_range(res_nfo_t nfo, bool claim_already_held) {

    nfo = round_nfo_to_page(nfo);

#if (NO_CACHE)
    if(claim_already_held) return 0;
    else return mem_claim(nfo.base, nfo.length, 1, own_mop);
#else

    return add_claims_to_table(&l0_table, nfo.base, nfo.length, claim_already_held);

#endif

}

void free_range(res_nfo_t nfo) {
    nfo = round_nfo_to_page(nfo);

#if (NO_CACHE)
    int res = mem_release(nfo.base, nfo.length, 1, own_mop);
    if(res != 0) {
        printf("Bad free, error code: %d\n", res);
    }
    assert(res == 0);
#else

    remove_claims_from_table(&l0_table, nfo.base, nfo.length);

#endif
}


// Allocation tracking, none of this is concurrent

// FIXME: I have not been careful about keeping reservations metadata in the same page
// FIXME: This means it is possible for metadata to be unmapped too early
// FIXME: If calling rescap_take ever gives a memory already freed error this is to blame
static res_nfo_t memhandle_nfo(capability mem) {
    register_t type = cheri_gettype(mem);
    return (type == RES_TYPE) ? rescap_nfo(mem) : MAKE_NFO(cheri_getlen(mem), cheri_getbase(mem));
}

#define BIG_OBJECT_THRESHOLD 0xe000
#define N_SLABS 47

// The sizes of objects
constexpr const size_t sizes[47] = { //skips 1 (0x3), 3 (0x5,6,7), 3 (10,12,14), 3 (20, 24, 28), 1 (40) ,1 (56) bucket sizes the nano kernel supports
        0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x30,
        0x40, 0x50, 0x60, 0x70, 0x80, 0xa0, 0xc0, 0xe0, 0x100, 0x140, 0x180, 0x1c0, 0x200, 0x280, 0x300, 0x380,
        0x400, 0x500, 0x600, 0x700, 0x800, 0xa00, 0xc00, 0xe00, 0x1000, 0x1400, 0x1800, 0x1c00, 0x2000, 0x2800, 0x3000, 0x3800,
        0x4000, 0x5000, 0x6000, 0x7000, 0x8000, 0xa000, 0xc000, 0xe000};

#define TOTAL_SKIPPED_SIZES 12

constexpr const uint8_t small_size_to_ndx[0x41] = {
        0,0, // (0,1)   ->      0x1
        1,   // (2)     ->      0x2
        2,2, // (3,4)   ->      0x4
        3,3,3,3, // (5..8) -> 0x8
        4,4,4,4,4,4,4,4, // (9..16) -> 0x10
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // (17, 32) -> 0x20
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6, // (33, 48) -> 0x30
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};    // (49, 64) -> 0x40

constexpr size_t size_to_ndx(size_t size) {
    return (size < 0x41) ? small_size_to_ndx[size] : (size_to_scale(size) - TOTAL_SKIPPED_SIZES);
}

constexpr size_t ndx_to_size(size_t size_ndx) {
    return sizes[size_ndx];
}

constexpr size_t ndx_to_total_calculate(size_t size_ndx) {
    /* Size of an object of this index */
    size_t object_size = ndx_to_size(size_ndx);

    /* Total space in slab that will not belong to the system */
    size_t total_size = SLAB_SIZE - MEM_REQUEST_FAST_OFFSET + RES_META_SIZE;

    /* How big a largest field of this size is */
    size_t field_size = RES_META_SIZE + (RES_SUBFIELD_BITMAP_BITS * object_size);

    /* How many fields in page rounded */
    size_t fields_per_page = (total_size + field_size - 1) / field_size;

    /* All meta data accounted for, round down so fits in page */
    size_t allocs_in_pool = (total_size - (fields_per_page * RES_META_SIZE)) / object_size;

    //return allocs_in_pool;
    return allocs_in_pool;
}

template<size_t N, size_t... Rest>
struct TotalsArray_t {
    static constexpr auto& value = TotalsArray_t<N - 1, ndx_to_total_calculate(N), Rest...>::value;
};

template<size_t... Rest>
struct TotalsArray_t<0, Rest...> {
    static constexpr size_t value[] = {ndx_to_total_calculate(0), Rest... };
};

template<size_t... Rest>
constexpr size_t TotalsArray_t<0, Rest...>::value[];

TotalsArray_t<N_SLABS-1> totals_ar;

constexpr const uint8_t small_ndx_to_scale[8] = {0,1,3,7,11,15,17,19};

constexpr size_t ndx_to_scale(size_t size_ndx) {
    return size_ndx < 8 ? small_ndx_to_scale[size_ndx] : size_ndx + TOTAL_SKIPPED_SIZES;
}

constexpr size_t ndx_to_total(size_t size_ndx) {
    return totals_ar.value[size_ndx];
}

class Slab {
    res_t current_field;
    res_t next_field;
    size_t current_ndx;
    size_t total_ndx;
    uint64_t at_addr;
    uint64_t meta_claim_current;
    uint64_t meta_claim_end;
    size_t dma_offset;

    void new_field(size_t size_ndx, res_t from_res, size_t dma_addr, size_t total) {

        current_field = from_res;

        // Not all objects can fit in one field.
        if(total > RES_SUBFIELD_BITMAP_BITS) {
            next_field = rescap_split(current_field, RES_SUBFIELD_BITMAP_BITS * ndx_to_size(size_ndx));
        }

        current_ndx = 0;
        total_ndx = 0;

        res_nfo_t nfo = rescap_nfo(current_field);

        if(dma_addr) {
            dma_addr -= (nfo.base - RES_META_SIZE);
        }

        dma_offset = dma_addr;

        at_addr = (uint64_t)nfo.base;

        meta_claim_current = nfo.base & ~(UNTRANSLATED_PAGE_SIZE-1);
        meta_claim_end = meta_claim_current + SLAB_SIZE;

        claim_range(MAKE_NFO(SLAB_SIZE, meta_claim_current), true);

        rescap_splitsub(current_field, ndx_to_scale(size_ndx));
    }

    void new_field(size_t size_ndx, bool dma, size_t total) {

        res_t new_res;
        size_t dma_off = 0;

        if (dma) {
            new_res = mem_request_phy_out(0, SLAB_SIZE - MEM_REQUEST_FAST_OFFSET, COMMIT_DMA, own_mop, &dma_off).val;
        } else {
            new_res = mem_request(0, SLAB_SIZE - MEM_REQUEST_FAST_OFFSET, NONE, own_mop).val;
        }

        new_field(size_ndx, new_res, dma_off, total);
    }

public:
    constexpr Slab() : current_field(nullptr), next_field(nullptr), current_ndx(0), total_ndx(0),
                        at_addr(0), meta_claim_current(0), meta_claim_end(0), dma_offset(0) {}

    res_t allocate(size_t size_ndx, bool dma, size_t* dma_off) {

        size_t total = ndx_to_total(size_ndx);
        size_t ob_size = ndx_to_size(size_ndx);

        // Get new memory if needed
        if(current_field == nullptr) {
            new_field(size_ndx, dma, total);
        }

        // Get our object

        res_t result = rescap_getsub(current_field, current_ndx);

        if(dma && dma_off) *dma_off = dma_offset;

        // And finally do some bookkeeping

        claim_range(MAKE_NFO(ob_size, at_addr), false);

        if(!IN_SAME_PAGE(cheri_getbase(current_field), at_addr)) {
            // The metadata node is in another page and so we place a second claim there

            claim_range(MAKE_NFO(RES_META_SIZE, cheri_getbase(current_field)), false);
        }

        at_addr += ob_size;

        if(++total_ndx == total) {
            // Finished with entire slab
            free_range(MAKE_NFO(meta_claim_end-meta_claim_current, meta_claim_current));

            current_field = nullptr;
        } else if(++current_ndx == RES_SUBFIELD_BITMAP_BITS) {
            // Finished with a chunk of the slab

            current_field = next_field;
            at_addr += RES_META_SIZE;

            if(total - total_ndx > RES_SUBFIELD_BITMAP_BITS) {
                next_field = rescap_split(current_field, RES_SUBFIELD_BITMAP_BITS * ob_size);
            }

            current_ndx = 0;

            rescap_splitsub(current_field,  ndx_to_scale(size_ndx));
        }

        uint64_t next_meta_claim = (cheri_getbase(current_field)) & ~(UNTRANSLATED_PAGE_SIZE-1);

        // We no longer need a claim for our metadata, can release
        if(current_field && (meta_claim_current != next_meta_claim)) {
            free_range(MAKE_NFO(next_meta_claim-meta_claim_current, meta_claim_current));
            meta_claim_current = next_meta_claim;
        }

        return result;
    }
};

struct Arena {
    DLL_LINK(Arena);

    bool dma;
    Slab slabs[N_SLABS];

    res_t allocate_with_request(size_t size, size_t* dma_off, bool align) {
        if(try_init_memmgt_ref() == NULL) return NULL;

        size_t aligned_size = size;
        precision_rounded_length pr;

        if(align) {
            // TODO cram/crap instructions
            pr = round_cheri_length(align_up_to(size,RES_META_SIZE));

            aligned_size = pr.length;
        }

        res_t res;

        if(!dma_off) {
            res = mem_request(0, aligned_size, COMMIT_NOW, own_mop).val;
        } else {
            res = mem_request_phy_out(0, aligned_size, (mem_request_flags)(COMMIT_NOW | COMMIT_DMA), own_mop, dma_off).val;
        }

        if(align && (pr.mask != 0)) {

            res_nfo_t nfo = rescap_nfo(res);

            // Align base
            size_t drop = (-nfo.base) & pr.mask;
            nfo.length -= drop;

            if(drop != 0) {
                res = rescap_split(res, drop-RES_META_SIZE);
            }

            // Align length
            if(nfo.length != pr.length) {
                rescap_split(res, pr.length);
            }
        }

        return res;
    }

public:

    constexpr Arena (bool is_dma) : next(nullptr), prev(nullptr), dma(is_dma) {}

    void setDma(bool is_dma) {
        dma = is_dma;
    }

    void initialise();

    // Why not a destructor? Just thread-local things.
    void removeFromList();

    res_t allocate(size_t size, size_t* dma_off, bool requires_split) {

        if(requires_split || (size > BIG_OBJECT_THRESHOLD)) {
            return allocate_with_request(size, dma_off, !requires_split);
        }

        size_t size_ndx = size_to_ndx(size);

        return slabs[size_ndx].allocate(size_ndx, dma, dma_off);
    }

};

__thread Arena default_arena(0);

__thread struct ArenaList {
    DLL(Arena);
    constexpr ArenaList(Arena* f, Arena* l) : first(f), last(l) {}
} arena_list(nullptr, nullptr);

void Arena::initialise() {
    DLL_ADD_START(&arena_list, this);
}

void Arena::removeFromList() {
    DLL_REMOVE(&arena_list, this);
}

__BEGIN_DECLS

void init_cap_malloc(void) {
    default_arena.initialise();
}

struct arena_t* new_arena(int dma) {
    Arena* A = (Arena*)malloc(sizeof(Arena));
    A->setDma(dma != 0);
    A->initialise();
    return (struct arena_t*)A;
}

VIS_EXTERNAL
void cap_free(capability mem) {
    res_nfo_t nfo = memhandle_nfo(mem);

    if(nfo.length > BIG_OBJECT_THRESHOLD) {
        nfo = round_nfo_to_page(nfo);
        mem_release(nfo.base, nfo.length, 1, own_mop);
        return;
    }

    free_range(nfo);

    if((cheri_gettype(mem) == RES_TYPE) && !IN_SAME_PAGE(cheri_getbase(mem), nfo.base)) {
        free_range(MAKE_NFO(RES_META_SIZE, cheri_getbase(mem)));
    }
}

VIS_EXTERNAL
void cap_free_handle(res_t res) {
    res_nfo_t nfo = rescap_nfo(res);
    if(!IN_SAME_PAGE(cheri_getbase(res), nfo.base)) {
        free_range(MAKE_NFO(RES_META_SIZE, cheri_getbase(res)));
    }
}

VIS_EXTERNAL
int cap_claim(capability mem) {
    res_nfo_t nfo = memhandle_nfo(mem);

    int res = claim_range(nfo, false);

    // Also claim the metadata if claim is called on reservation
    if(res == 0 && (cheri_gettype(mem) == RES_TYPE) && !IN_SAME_PAGE(cheri_getbase(mem), nfo.base)) {
        res = claim_range(MAKE_NFO(RES_META_SIZE,cheri_getbase(mem)), false);
    }

    return res;
}

res_t cap_malloc_arena_dma_requires(size_t size, struct arena_t* arena, size_t* dma_off, int requires_split) {
    res_t r = ((Arena*)arena)->allocate(size, dma_off, requires_split != 0);
    return r;
}

res_t cap_malloc_arena_dma(size_t size, struct arena_t* arena, size_t* dma_off) {
    return cap_malloc_arena_dma_requires(size, arena, dma_off, false);
}

VIS_EXTERNAL
res_t  cap_malloc(size_t size) {
    return cap_malloc_arena_dma_requires(size, (struct arena_t*)&default_arena, NULL, 0);
}

VIS_EXTERNAL
res_t  cap_malloc_need_split(size_t size) {
    return cap_malloc_arena_dma_requires(size, (struct arena_t*)&default_arena, NULL, 1);
}

res_t cap_malloc_arena(size_t size, struct arena_t* arena) {
    return cap_malloc_arena_dma_requires(size, arena, NULL, false);
}

__END_DECLS