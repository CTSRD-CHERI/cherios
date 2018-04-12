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

/* Resource counting is done on a PAGE basis, and is left completely to the system. Double free will therefore go
 * un-noticed until a page is freed too early. However, due to temporal safety, accidental free / double free
 * will result in either a) A page fault, b) access to a safe object. One will result in clean exit (as you have a
 * program error), the other in continued proper function of the program. Either way we are safe. */

/* 'Very big' objects (on the order of magnitude of the pool size) are just handled by the system directly */

/* TODO: Currently every thread has its own allocation pool, and there is a single free thread. Might want to
 * TODO: evaluate that idea */

/* TODO: Alleviate some burden from the system by caching claims for objects in the same range */

#include "capmalloc.h"
#include "math.h"
#include "nano/nanotypes.h"
#include "object.h"
#include "mman.h"
#include "assert.h"
#include "thread.h"
#include "stdio.h"

typedef struct fixed_pool {
    res_t field;
    res_t rest;

    size_t field_ndx;
    size_t total_ndx;
    size_t pool_size;

    // When we stop using this pool, claim the range this many times
    size_t outstanding_claims;
    size_t start;
    size_t end;
} fixed_pool;

typedef struct dypool {
    res_t head;
    size_t length;

    size_t page_start;
    size_t page_offset;

    size_t outstanding_claims;

} dypool;

#define N_FIXED_POOLS 10
#define FIXED_POOL_RES_N
#define DYNAMIC_POOL_SIZE ((1 << 30) - RES_META_SIZE)
#define FIXED_POOL_SIZE   (UNTRANSLATED_PAGE_SIZE -  RES_META_SIZE)

#define BIG_OBJECT_THRESHOLD (1 << 27)

__thread fixed_pool pools[N_FIXED_POOLS];
__thread dypool dynamic_pool;

act_kt worker_act = NULL;


void worker_start(register_t arg, capability carg) {
    worker_act = act_self_ref;

    /* Simple message read loop that calls mem_release/claim on behalf of cap_free */

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

        if(msg->c1 != NULL) message_reply(NULL, result, 0, msg->c2, msg->c1);

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

static int offload_claim(size_t base, size_t length, size_t times, mop_t mop, ccall_selector_t mode) {
    if(proc_handle == NULL) return mem_claim(base, length, times, mop);

    if(worker_act == NULL) try_make_worker();

    return (int)message_send(base, length, times, 0, mop, NULL, NULL, NULL, worker_act, mode, 1);
}

static int offload_release(size_t base, size_t length, size_t times, mop_t mop, ccall_selector_t mode) {
    if(proc_handle == NULL) return mem_release(base, length, times, mop);

    if(worker_act == NULL) try_make_worker();

    return (int)message_send(base, length, times, 0, mop, NULL, NULL, NULL, worker_act, mode, 0);
}

static capability memhandle_nfo(capability mem) {
    register_t type = cheri_gettype(mem);
    if(type == RES_TYPE) {
        mem = rescap_info(mem);
    }
    return mem;
}

static res_t alloc_from_pool(size_t size, size_t pool_n) {
    fixed_pool* p = &pools[pool_n];

    if(p->field == NULL) {
        /* Needs a new page. We might eventually try make this asyc as well (have a queue of pages waiting */
        if(try_init_memmgt_ref() == NULL) return NULL;

        p->field = mem_request(0,FIXED_POOL_SIZE - RES_META_SIZE, NONE, own_mop).val;

        assert(p->field != NULL);

        p->start = align_down_to(cheri_getbase(rescap_info(p->field)), UNTRANSLATED_PAGE_SIZE);
        p->end = p->start + UNTRANSLATED_PAGE_SIZE;

        if(p->pool_size > RES_SUBFIELD_BITMAP_BITS) {
            p->rest = rescap_split(p->field, RES_SUBFIELD_BITMAP_BITS << pool_n);
        }

        p->field_ndx = 0;
        p->total_ndx = 0;

        p->outstanding_claims = 0;

        rescap_splitsub(p->field, pool_n);
    }

    res_t result = rescap_getsub(p->field, p->field_ndx);
    p->outstanding_claims ++;

    if(++(p->total_ndx) == p->pool_size) {
        /* Finished with this page */

        p->field = NULL;

        /* We send one less claim as we had the initial claim (for the metadata) which we no longer need */
        if(p->outstanding_claims > 1) offload_claim(p->start, UNTRANSLATED_PAGE_SIZE, p->outstanding_claims-1, own_mop, SEND);
    } else if(++(p->field_ndx) == RES_SUBFIELD_BITMAP_BITS) {
        /* Start a new subfield */

        p->field = p->rest;

        if(p->pool_size - p->total_ndx > RES_SUBFIELD_BITMAP_BITS) {
            p->rest = rescap_split(p->field, RES_SUBFIELD_BITMAP_BITS << pool_n);
        }

        p->field_ndx = 0;

        rescap_splitsub(p->field, pool_n);
    }

    return result;
}

/*

void finish_dynamic_pool(void) {
    if(dynamic_pool.outstanding_claims == 0) {
        offload_release(dynamic_pool.page_start + dynamic_pool.page_offset, dynamic_pool.length, 1, own_mop, SEND);
    } else {

        // We may have some claims in the current page
        if(dynamic_pool.outstanding_claims > 1) {
            offload_claim(dynamic_pool.page_start, UNTRANSLATED_PAGE_SIZE, dynamic_pool.outstanding_claims - 1, own_mop, SEND);
        }

        // We will have none in the space we are throwing away
        size_t next_page_start = dynamic_pool.page_start + UNTRANSLATED_PAGE_SIZE;
        size_t next_length = dynamic_pool.length + dynamic_pool.page_offset - UNTRANSLATED_PAGE_SIZE;

        if(next_length != 0) {
            offload_release(next_page_start, next_length, 1, own_mop, SEND);
        }
    }
}

static res_t alloc_from_dynamic(size_t size) {
    size_t aligned_size = align_up_to(size, RES_META_SIZE);
    size_t size_with_meta = aligned_size + RES_META_SIZE;

    if(dynamic_pool.head == NULL || dynamic_pool.length < size_with_meta) {

        if(dynamic_pool.head != NULL) {
            // Finish off the pool. It is not big enough to allocate the next object
            finish_dynamic_pool();
        }

        // Need new pool
        dynamic_pool.head = mem_request(0, DYNAMIC_POOL_SIZE - RES_META_SIZE, NONE, own_mop);

        dynamic_pool.length = DYNAMIC_POOL_SIZE;

        dynamic_pool.page_start = align_down_to(cheri_getbase(rescap_info(dynamic_pool.head)), UNTRANSLATED_PAGE_SIZE);
        dynamic_pool.page_offset = UNTRANSLATED_PAGE_SIZE;

        dynamic_pool.outstanding_claims = 0;
    }

    res_t head = dynamic_pool.head;
    res_t tail = rescap_split(head, aligned_size);




    if(dynamic_pool.length < (1 << (N_FIXED_POOLS-1))) {
        finish_dynamic_pool();
    } else if(dynamic_pool.page_offset == UNTRANSLATED_PAGE_SIZE - RES_META_SIZE) {
        dynamic_pool.head = rescap_split(dynamic_pool.head, 0);
        dynamic_pool.length -= RES_META_SIZE;
        dynamic_pool.page_offset = 0;
        dynamic_pool.page_start += UNTRANSLATED_PAGE_SIZE;
    }

    return head;
}
*/

static res_t allocate_with_request(size_t size) {
    if(try_init_memmgt_ref() == NULL) return NULL;
    size_t aligned_size = align_up_to(size, RES_META_SIZE);
    res_t res = mem_request(0, aligned_size, NONE, own_mop).val;
    return res;
}

res_t cap_malloc(size_t size) {

#define CASE_X(x) if(size <= (1 << x)) return alloc_from_pool(size, x);

    /* Allocate from a fixed size pool if object is small */

    CASE_X(0);
    CASE_X(1);
    CASE_X(2);
    CASE_X(3);
    CASE_X(4);
    CASE_X(5);
    CASE_X(6);
    CASE_X(7);
    CASE_X(8);
    CASE_X(9);

    if(size < BIG_OBJECT_THRESHOLD) {

        // Allocate from bumping buffer for medium sized objects
        // return alloc_from_dynamic(size);

        return allocate_with_request(size);

    } else {

        // Allocate by calling request for big objects

        return allocate_with_request(size);

    }
}

fixed_pool* find_inuse(capability nfo) {
    size_t base = cheri_getbase(nfo);

    for(size_t i = 0; i < N_FIXED_POOLS; i++) {
        fixed_pool* p = & pools[i];

        if(p->field != NULL && base >= p->start && base < p->end) {
            return p;
        }
    }

    return NULL;
}

int cap_claim(capability mem) {
    capability nfo = memhandle_nfo(mem);

    fixed_pool* fixed = find_inuse(nfo);

    if(fixed != NULL) {
        fixed->outstanding_claims++;
        return MEM_OK;
    } else {
        // TODO a cache for frequent claim/free

        // Sync because we care about the return

        return offload_claim(cheri_getbase(nfo), cheri_getlen(nfo), 1, own_mop, SYNC_CALL);
    }
}

void cap_free(capability mem) {
    capability nfo = memhandle_nfo(mem);

    fixed_pool* fixed = find_inuse(nfo);

    if(fixed != NULL) {
        if(fixed->outstanding_claims == 0) {
            assert(0 && "Double free noticed\n");
        }
        fixed->outstanding_claims--;
        return;
    } else {

        // TODO a cache for frequent claim/free

        // We free asynchronously for efficiency

        offload_release(cheri_getbase(nfo), cheri_getlen(nfo), 1, own_mop, SEND);
    }
}

void init_cap_malloc(void) {
    /* If we are a process it stands to reason we can create threads. Otherwise we can't use free =( */

    dynamic_pool.head = NULL;

    for(size_t i = 0; i < N_FIXED_POOLS; i++) {
        pools[i].field = NULL;

        /* Space in one page minus systems part */
        size_t page_size = FIXED_POOL_SIZE;

        /* How big a largest field of this size is */
        size_t field_size = RES_META_SIZE + (RES_SUBFIELD_BITMAP_BITS << i);

        /* How many fields in page rounded */
        size_t fields_per_page = (page_size + field_size - 1) / field_size;

        /* All meta data acounted for, round down so fits in page */
        size_t allocs_in_pool = (page_size - (fields_per_page * RES_META_SIZE)) >> i;

        /* How many allocations we can make in a pool of this size */
        pools[i].pool_size = allocs_in_pool;
    }
}