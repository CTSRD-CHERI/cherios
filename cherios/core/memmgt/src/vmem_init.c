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
#include "math.h"
#include "mmap.h"
#include "pmem.h"

static mop_t init_vmem(capability a_mop_sealing_cap) {
    /* This creates enough virtual memory to get started */
    ptable_t top_table, L1_0, L2_0;

    top_table = get_top_level_table();
    assert(top_table != NULL);
    CHERI_PRINT_CAP(top_table);
    L1_0 = vmem_create_table(top_table, 0, 1);
    assert(L1_0 != NULL);
    CHERI_PRINT_CAP(L1_0);
    L2_0 = vmem_create_table(L1_0, 0, 2);
    assert(L2_0 != NULL);
    CHERI_PRINT_CAP(L2_0);

    int res = vmem_create_mapping(L2_0, 0, TLB_FLAGS_DEFAULT);
    assert(res == 0);
    res = vmem_create_mapping(L2_0, 1, TLB_FLAGS_DEFAULT);
    assert(res == 0);

    /* Now we get the first reservation which NEEDS virtual mem */
    res_t first = make_first_reservation();

    res_nfo_t nfo = rescap_nfo(first);

    /* Align the reservation to a page boundry (just throw away forever the first few bytes) */
    size_t base = nfo.base;

    size_t realigned_base = align_up_to(base - RES_META_SIZE, UNTRANSLATED_PAGE_SIZE) + RES_META_SIZE;

    if(base != realigned_base) {
        size_t diff = realigned_base-base;
        first = rescap_split(first, diff);
    }

    /* Intialising memory ownership tracking */
    return __init_mop(a_mop_sealing_cap, first);
}

static void init_book(void) {
    book = get_book();
}

mop_t mem_minit(capability a_mop_sealing_cap) {
    printf("Getting book\n");
    init_book();
    pmem_condense_book();
    pmem_check_book();
    printf("Starting up virtual memory and reservation system\n");
    return init_vmem(a_mop_sealing_cap);
}