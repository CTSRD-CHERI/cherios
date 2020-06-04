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


/* Simple bumping allocator. Uses a worker thread for free to avoid delay. Small objects all allocated from
 * separate pools, otherwise there is a single buffer. */

/* Freed ranges are remembered in a linked list until the last object is freed. The system is only notified
 * when a range is completely finished with. For objects from other mallocs we currently don't cache any claims */

/* Resource counting is done on a range basis. Double free will therefore go un-noticed until a range is
 * freed too early. However, due to temporal safety, accidental free / double free
 * will result in either a) A page fault, b) access to a safe object. One will result in clean exit (as you have a
 * program error), the other in continued proper function of the program. Either way we are safe. */

/* 'Very big' objects (on the order of magnitude of the pool size) are just handled by the system directly */

/* TODO: Currently every thread has its own allocation pool, and there is a single free thread. Might want to
 * TODO: evaluate that idea */

/* TODO: Alleviate some burden from the system by caching claims for objects in the same range */

#include <nano/nanokernel.h>
#include "capmalloc.h"
#include "math.h"
#include "nano/nanotypes.h"
#include "object.h"
#include "mman.h"
#include "assert.h"
#include "thread.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "lists.h"

// Keeps track of claims in a bump the pointer allocator
typedef struct dy_pool_range {
    size_t start;
    size_t end;
    size_t outstanding_claims;
} dy_pool_range_t;

typedef struct fixed_pool {
    res_t field;
    res_t rest;

    size_t object_size;

    size_t field_ndx;
    size_t total_ndx;
    size_t pool_size;

    size_t dma_off;

    // When we stop using this pool, claim the range this many times
    dy_pool_range_t range;
} fixed_pool;



typedef struct old_claim_t {
    struct old_claim_t* next;
    dy_pool_range_t range;
} old_claim_t;

#define COMMIT_CHUNK_MIN 0x2000
#define COMMIT_CHUNK_MAX 0x800000
typedef struct dypool {
    res_t head;
    size_t length;
    size_t res_at_base;
    size_t commited_to;

    size_t commit_chunk;

    dy_pool_range_t range;
    size_t dma_off;
} dypool;


// Examples for N pools and largest fixed object are (16, 0x20), (32, 0x200), (40, 0x800)

#define N_FIXED_POOLS 32

#define LARGEST_FIXED_OBJECT 0x200

#define DYNAMIC_POOL_SIZE ((1 << 26) - RES_META_SIZE) // This is OK for late commit. This is stupidly large for commit!
#define DYNAMIC_POOL_SIZE_DMA ((1 << 26) - RES_META_SIZE) // Still pretty big!

#define FIXED_POOL_SIZE   ((64 * UNTRANSLATED_PAGE_SIZE) -  RES_META_SIZE)

// Need less faulting for bump the pointer
#define BIG_OBJECT_THRESHOLD (1 << 20)
#define BIG_OBJECT_THRESHOLD_DMA (1 << 20)

// We could use a worker to do some work in off cycles, but this was never implemented
#define USE_WORKER 0

__thread fixed_pool pools[N_FIXED_POOLS];

typedef struct arena_t {
    DLL_LINK(arena_t);
    fixed_pool pools[N_FIXED_POOLS];
    struct old_claim_t* old_fixed_pools_heads[N_FIXED_POOLS];
    dypool dynamic_pool;
    struct old_claim_t* old_pools_head;
    int dma;
} arena_t;

__thread arena_t default_arena;
__thread struct {
    DLL(arena_t);
} arena_list;

#if (USE_WORKER)
act_kt worker_act = NULL;

void worker_start(__unused register_t arg, __unused capability carg) {
    worker_act = act_self_ref;

    /* Simple message read loop that calls mem_release/claim on behalf of cap_free */

    syscall_change_priority(act_self_ctrl, PRIO_LOW);

    while(1) {
        msg_t* msg = get_message();

        size_t base = msg->a0;
        size_t length = msg->a1;
        size_t times = msg->a2;

        mop_t mop = msg->c3;

        int mode = (int)msg->v0;

        int result;

        if(mode == 0) result = mem_release(base, length, times, mop);
        else result = mem_claim(base, length, times, mop);

        if(msg->c1 != NULL) message_reply(NULL, result, 0, msg->c1, 1);

        next_msg();
    }
}

void try_make_worker(void) {
    const char* name = syscall_get_name(act_self_ref);
    char wrkrname[] = "____wrkr";
    wrkrname[0] = name[0];
    wrkrname[1] = name[1];
    wrkrname[2] = name[2];

    thread_new(wrkrname, 0, NULL, &worker_start);
    /* Yield until our worker has registered */

    while(worker_act == NULL) {
        sleep(0);
    }
}



static inline int offload_claim(size_t base, size_t length, size_t times, mop_t mop, ccall_selector_t mode) {

    if(proc_handle == NULL) return mem_claim(base, length, times, mop);

    if(worker_act == NULL) try_make_worker();

    return (int)message_send(base, length, times, 0, mop, NULL, NULL, NULL, worker_act, mode, 1);


}

static int offload_release(size_t base, size_t length, size_t times, mop_t mop, ccall_selector_t mode) {
    if(proc_handle == NULL) return mem_release(base, length, times, mop);

    if(worker_act == NULL) try_make_worker();

    return (int)message_send(base, length, times, 0, mop, NULL, NULL, NULL, worker_act, mode, 0);
}

#else

#define offload_claim mem_claim_mode
#define offload_release mem_release_mode

#endif

#define CLAIM_BLOCK_SIZE (UNTRANSLATED_PAGE_SIZE - RES_META_SIZE)
#define CLAIMS_PER_BLOCK (CLAIM_BLOCK_SIZE / sizeof(struct old_claim_t))


// Small reusing slab allocator for this specific structure. Don't want to use malloc in malloc.

__thread old_claim_t* old_claim_block;
__thread old_claim_t* claim_free_head;

static inline struct old_claim_t* malloc_old_claim(void) {
    old_claim_t* claim = claim_free_head;
    old_claim_t* new_head;
    if(cheri_gettag(claim)) {
        // Is a pointer
        new_head = claim->next;
    } else {
        // Is an index into the block
        size_t ndx = (size_t)claim;
        if(old_claim_block == NULL) {
            ERROR_T(res_t) res = mem_request(0, CLAIM_BLOCK_SIZE, COMMIT_NOW, own_mop);
            assert(IS_VALID(res));
            _safe cap_pair pair;
            rescap_take(res.val, &pair);
            old_claim_block = (old_claim_t*)pair.data;
        }
        claim = &old_claim_block[ndx++];
        if(ndx == CLAIMS_PER_BLOCK) {
            old_claim_block = NULL;
            ndx = 0;
        }
        new_head = (old_claim_t*)(uintptr_t)ndx;
    }

    claim_free_head = new_head;
    return claim;
}

static inline void free_old_claim(struct old_claim_t* claim) {
    claim->next = claim_free_head;
    claim_free_head = claim;
}

static res_nfo_t memhandle_nfo(capability mem) {
    register_t type = cheri_gettype(mem);
    return (type == RES_TYPE) ? rescap_nfo(mem) : MAKE_NFO(cheri_getlen(mem), cheri_getbase(mem));
}

static void release_range(dy_pool_range_t* range) {
    offload_release(range->start, range->end - range->start, 1, own_mop, SEND);
}

static void finish_dynamic_pool(arena_t* arena) {
    dypool* dynamic_pool = &arena->dynamic_pool;

    if(dynamic_pool->range.outstanding_claims == 0) {
        release_range(&dynamic_pool->range);
    } else {
        old_claim_t* new_old = malloc_old_claim();
        new_old->range = dynamic_pool->range;
        new_old->next = arena->old_pools_head;
        arena->old_pools_head = new_old;
    }

    dynamic_pool->head = NULL;
}

static void finish_static_pool(arena_t* arena, size_t pool_n) {

    old_claim_t* new_old = malloc_old_claim();
    new_old->range = arena->pools[pool_n].range;
    new_old->next =  arena->old_fixed_pools_heads[pool_n];
    arena->old_fixed_pools_heads[pool_n] = new_old;

}

static res_t alloc_from_pool(__unused size_t size, size_t pool_n, arena_t* arena, size_t* dma_off) {
    fixed_pool* p = &arena->pools[pool_n];

    if(p->field == NULL) {
        /* Needs a new page. We might eventually try make this asyc as well (have a queue of pages waiting */

        ERROR_T(res_t) res;

        if(arena->dma) {
            res = mem_request_phy_out(0,FIXED_POOL_SIZE - RES_META_SIZE, COMMIT_DMA, own_mop, &p->dma_off);
        } else {
            res = mem_request(0,FIXED_POOL_SIZE - RES_META_SIZE, NONE, own_mop);
            p->dma_off = 0;
        }

        if(!IS_VALID(res)) {
            printf(KRED"Mem request failed with code %d\n"KRST, (int)res.er);
            assert(IS_VALID(res));
        }

        p->field = res.val;
        res_nfo_t nfo = rescap_nfo(res.val);

        if(arena->dma) {
            p->dma_off -= (nfo.base-RES_META_SIZE);
        }

        p->range.start = align_down_to(nfo.base, UNTRANSLATED_PAGE_SIZE);
        p->range.end = p->range.start + FIXED_POOL_SIZE + RES_META_SIZE;

        if(p->pool_size > RES_SUBFIELD_BITMAP_BITS) {
            p->rest = rescap_split(p->field, RES_SUBFIELD_BITMAP_BITS * p->object_size);
        }

        p->field_ndx = 0;
        p->total_ndx = 0;

        p->range.outstanding_claims = 0;

        rescap_splitsub(p->field, pool_n);
    }

    res_t result = rescap_getsub(p->field, p->field_ndx);
    p->range.outstanding_claims ++;

    if(arena->dma && dma_off) *dma_off = p->dma_off;

    if(++(p->total_ndx) == p->pool_size) {
        /* Finished with this page */

        p->field = NULL;

        /* We send one less claim as we had the initial claim (for the metadata) which we no longer need */
        finish_static_pool(arena, pool_n);

    } else if(++(p->field_ndx) == RES_SUBFIELD_BITMAP_BITS) {
        /* Start a new subfield */

        p->field = p->rest;

        if(p->pool_size - p->total_ndx > RES_SUBFIELD_BITMAP_BITS) {
            p->rest = rescap_split(p->field, RES_SUBFIELD_BITMAP_BITS * p->object_size);
        }

        p->field_ndx = 0;

        rescap_splitsub(p->field, pool_n);
    }

    return result;
}

static res_t alloc_from_dynamic(size_t size, arena_t* arena, size_t* dma_off) {

    dypool* dynamic_pool = &arena->dynamic_pool;

    precision_rounded_length pr = round_cheri_length(align_up_to(size,RES_META_SIZE));

    size_t aligned_size = pr.length;
    size_t size_with_meta = aligned_size + RES_META_SIZE;

    if(dynamic_pool->head == NULL || dynamic_pool->length < size_with_meta + pr.mask) {

        if(dynamic_pool->head != NULL) {
            // Finish off the pool. It is not big enough to allocate the next object
            finish_dynamic_pool(arena);
        }

        size_t pool_size = arena->dma ? DYNAMIC_POOL_SIZE_DMA : DYNAMIC_POOL_SIZE;
        ERROR_T(res_t) res;
        // Need new pool
        if(arena->dma) {
           res = mem_request_phy_out(0, pool_size - RES_META_SIZE, COMMIT_DMA, own_mop, &dynamic_pool->dma_off);
        } else {
           res = mem_request(0, pool_size - RES_META_SIZE, NONE, own_mop);
        }

        assert(IS_VALID(res));

        dynamic_pool->head = res.val;

        dynamic_pool->length = pool_size;

        res_nfo_t nfo = rescap_nfo(dynamic_pool->head);

        if(arena->dma) {
            dynamic_pool->dma_off -= (nfo.base - RES_META_SIZE);
        }

        dynamic_pool->range.start = align_down_to(nfo.base, UNTRANSLATED_PAGE_SIZE);
        dynamic_pool->range.end = dynamic_pool->range.start + pool_size + RES_META_SIZE;
        dynamic_pool->range.outstanding_claims = 0;

        dynamic_pool->res_at_base = nfo.base;

        dynamic_pool->commited_to = arena->dma ?
                                    dynamic_pool->range.end :
                                    align_up_to(nfo.base, UNTRANSLATED_PAGE_SIZE);
    }

    res_t head = dynamic_pool->head;

    size_t rounded_base = (dynamic_pool->res_at_base + pr.mask) & ~pr.mask;
    size_t skip = rounded_base - dynamic_pool->res_at_base;
    size_with_meta +=skip;

    size_t new_at_base = dynamic_pool->res_at_base + size_with_meta;
    size_t commited_to = dynamic_pool->commited_to;
    size_t in_chunks = dynamic_pool->commit_chunk;

    if(new_at_base > commited_to) {
        do {
            size_t end = commited_to + in_chunks;
            end = end > dynamic_pool->range.end ? dynamic_pool->range.end : end;
            mem_commit_range(commited_to, (end - commited_to) >> UNTRANSLATED_BITS, 0);
            commited_to = end;
            if(in_chunks < COMMIT_CHUNK_MAX) in_chunks <<=1;
        } while(new_at_base > commited_to);
        dynamic_pool->commit_chunk = in_chunks;
        dynamic_pool->commited_to = commited_to;
    }


    if(skip) {
        head = rescap_split(head, skip - RES_META_SIZE);
    }
    res_t tail = rescap_split(head, aligned_size);

    if(arena->dma && dma_off) *dma_off = dynamic_pool->dma_off;
    dynamic_pool->head = tail;
    dynamic_pool->range.outstanding_claims++;
    dynamic_pool->res_at_base = new_at_base;
    dynamic_pool->length -=size_with_meta;

    // We are tracking over the entire pool range, not on a page bases, so this is not needed
    /* else if(dynamic_pool->page_offset == UNTRANSLATED_PAGE_SIZE - RES_META_SIZE) {
        // If we transfer claim management to the central allocator we must do this.
        dynamic_pool->head = rescap_split(dynamic_pool->head, 0);
        dynamic_pool->length -= RES_META_SIZE;
        dynamic_pool->page_offset = 0;
        dynamic_pool->page_start += UNTRANSLATED_PAGE_SIZE;
    } */

    return head;
}

static res_t allocate_with_request(size_t size, size_t* dma_off) {
    if(try_init_memmgt_ref() == NULL) return NULL;

    precision_rounded_length pr = round_cheri_length(align_up_to(size,RES_META_SIZE));

    size_t aligned_size = pr.length;

    res_t res;

    if(!dma_off) {
        res = mem_request(0, aligned_size, COMMIT_NOW, own_mop).val;
    } else {
        res = mem_request_phy_out(0, aligned_size, COMMIT_NOW | COMMIT_DMA, own_mop, dma_off).val;
    }

    if(pr.mask != 0) {

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

VIS_EXTERNAL
res_t  cap_malloc(size_t size) {
    return cap_malloc_arena_dma(size, &default_arena, NULL);
}

res_t cap_malloc_arena(size_t size, struct arena_t* arena) {
    return cap_malloc_arena_dma(size, arena, NULL);
}

res_t cap_malloc_arena_dma(size_t size, struct arena_t* arena, size_t* dma_off) {

    if(dma_off) assert(arena->dma);

    if(try_init_memmgt_ref() == NULL) return NULL;

    // This is require to ensure alignment of any type the returned object may be cast to
    if(size < CAP_SIZE) {
        round_up_to_nearest_power_2(size);
    }

    if(size <= LARGEST_FIXED_OBJECT) {
        size = size ? size : 1;
        uint32_t pool = size_to_scale((uint32_t)size);
        return alloc_from_pool(size, pool, arena, dma_off);
    }
    else if(size < (arena->dma ? BIG_OBJECT_THRESHOLD_DMA : BIG_OBJECT_THRESHOLD)) {

        // Allocate from bumping buffer for medium sized objects
        // return alloc_from_dynamic(size);

        //return allocate_with_request(size);
        return alloc_from_dynamic(size, arena, dma_off);
    } else {

        // Allocate by calling request for big objects

        return allocate_with_request(size, dma_off);

    }
}

static dy_pool_range_t* find_inuse(size_t base, size_t  length, old_claim_t*** previous_next_link) {

    if(length  >= BIG_OBJECT_THRESHOLD) {
        return NULL;
    }

    uint32_t ndx;

    if(length <= LARGEST_FIXED_OBJECT) {
        length = length ? length : 1;
        ndx = size_to_scale(length);
    }

    DLL_FOREACH(arena_t, arena, &arena_list) {

        old_claim_t** before;

        if(length <= LARGEST_FIXED_OBJECT) {
            fixed_pool* p = &arena->pools[ndx];
            if(p->field != NULL && base >= p->range.start && base < p->range.end) {
                return &p->range;
            }

            before = &arena->old_fixed_pools_heads[ndx];
        } else {
            if(arena->dynamic_pool.head != NULL && base >= arena->dynamic_pool.range.start && base < arena->dynamic_pool.range.end) {
                return &arena->dynamic_pool.range;
            }

            before = &arena->old_pools_head;
        }


        old_claim_t* old = *before;

        while(old != NULL) {
            if(base >= old->range.start && base < old->range.end) {
                if(previous_next_link) *previous_next_link = before;
                return &old->range;
            }
            before = &old->next;
            old = old->next;
        }

    }


    return NULL;
}

int cap_claim(capability mem) {
    res_nfo_t nfo = memhandle_nfo(mem);

    dy_pool_range_t* have_tracker = find_inuse(nfo.base, nfo.length, NULL);

    if(have_tracker) {
        have_tracker->outstanding_claims++;
        return MEM_OK;
    } else {
        // TODO a cache for frequent claim/free

        // Sync because we care about the return

        return offload_claim(nfo.base, nfo.length, 1, own_mop, SYNC_CALL);
    }
}

VIS_EXTERNAL
void cap_free(capability mem) {
    res_nfo_t nfo = memhandle_nfo(mem);

    old_claim_t** old_before = NULL;
    dy_pool_range_t* have_tracker = find_inuse(nfo.base, nfo.length, &old_before);

    if(have_tracker) {
        if(have_tracker->outstanding_claims == 0) {
            assert(0 && "Double free noticed\n");
        }
        have_tracker->outstanding_claims--;
        if(have_tracker->outstanding_claims == 0 && old_before) {
            offload_release(have_tracker->start, have_tracker->end - have_tracker->start, 1, own_mop, SEND);
            old_claim_t* to_free = *old_before;
            *old_before = (*old_before)->next;
            free_old_claim(to_free);
        }
        return;
    } else {

        // TODO a cache for frequent claim/free

        // We free asynchronously for efficiency

        offload_release(nfo.base, nfo.length, 1, own_mop, SEND);
    }
}

void init_fixed_pool(fixed_pool* pool, size_t object_size) {

    pool->field = NULL;

    /* Space in one page minus systems part */
    size_t page_size = FIXED_POOL_SIZE;

    /* How big a largest field of this size is */
    size_t field_size = RES_META_SIZE + (RES_SUBFIELD_BITMAP_BITS * object_size);

    /* How many fields in page rounded */
    size_t fields_per_page = (page_size + field_size - 1) / field_size;

    /* All meta data acounted for, round down so fits in page */
    size_t allocs_in_pool = (page_size - (fields_per_page * RES_META_SIZE)) / object_size;

    /* How many allocations we can make in a pool of this size */
    pool->pool_size = allocs_in_pool;

    pool->object_size = object_size;
}

static void init_arena(arena_t* arena, int dma) {
    /* If we are a process it stands to reason we can create threads. Otherwise we can't use free =( */

    arena->dynamic_pool.head = NULL;
    arena->dynamic_pool.commit_chunk = COMMIT_CHUNK_MIN;

    for(size_t i = 0; i < N_FIXED_POOLS; i++) {
        size_t object_size = scale_to_size(i);
        init_fixed_pool(&arena->pools[i], object_size);
    }

    arena->dma = dma;
    arena->old_pools_head = NULL;

    DLL_ADD_START(&arena_list, arena);
}

struct arena_t* new_arena(int dma) {
    arena_t* arena = (arena_t*)malloc(sizeof(arena_t));
    init_arena(arena, dma);
    return arena;
}

void init_cap_malloc(void) {
    init_arena(&default_arena, 0);
}