/*-
 * Copyright (c) 2014, 2016 Robert N. M. Watson
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

#include "stddef.h"
#include "cheric.h"
#include "string.h"
#include "dylink.h"
#include "crt.h"

typedef unsigned long long mips_function_ptr;
typedef void (*cheri_function_ptr)(void);


void	crt_init_bss(capability auth);
void	crt_call_constructors(void);

/*
 * In version 3 of the CHERI sandbox ABI, function pointers are capabilities.
 * The CTORs list is the single exception: CTORs are used to set up globals
 * that contain function pointers so (until we have proper linker support) we
 * are still generating them as a sequence of PCC-relative integers.
 */

static mips_function_ptr __attribute__((used))
    __attribute__((section(".ctors")))
    __CTOR_LIST__[1] = { (mips_function_ptr)(-1) };

static mips_function_ptr __attribute__((used))
    __attribute__((section(".dtors")))
    __DTOR_LIST__[1] = { (mips_function_ptr)(-1) };

extern void __cap_table_start;
extern void __cap_table_local_start;

size_t cap_relocs_size;
// Of the main thread. We should probably null out the TLS only bit before creating new threads.
// This is used for dedup in main thread. And also by the secure loader for making new threads.
capability crt_segment_table[MAX_SEGS];
size_t crt_segment_table_vaddrs[MAX_SEGS];
size_t crt_code_seg_offset;
size_t crt_tls_seg_size;

size_t crt_cap_tab_local_addr;
capability crt_tls_proto;
size_t crt_tls_proto_size;
size_t crt_tls_seg_off; // _offset_ (not index)

/*
 * Symbols provided by rtendC.c, which provide us with the tails for the
 * constructor and destructor arrays.
 */
extern mips_function_ptr __CTOR_END__;
extern mips_function_ptr __DTOR_END__;

/*
 * Execute constructors; invoked by the crt_sb.S startup code.
 *
 * NB: This code and approach is borrowed from the MIPS ABI, and works as long
 * as CHERI code generation continues to use 64-bit integers for pointers.  If
 * that changes, this might need to become more capability-appropriate.
 */

VIS_HIDDEN
void
crt_call_constructors(void)
{
	mips_function_ptr *func;

	for (func = &__CTOR_LIST__[0];
	    func != &__CTOR_END__;
	    func++) {
		if (*func != (mips_function_ptr)-1) {
			cheri_function_ptr cheri_func =
				(cheri_function_ptr)__builtin_cheri_offset_set(
						__builtin_cheri_program_counter_get(), *func);
			cheri_func();
		}
	}
}

#if (IS_BOOT == 1)

extern void	__start_bss;
extern void __bss_size;

#define BSS_START ((size_t)(&__start_bss))
#define BSS_SIZE ((size_t)(&__bss_size))

void
crt_init_bss(capability auth)
{
    // Boot BSS may not be zero
	char* bss = cheri_incoffset(auth, BSS_START - (size_t)auth);
	bzero(bss, BSS_SIZE);
}

#else // Not boot

void
crt_init_bss(__unused capability auth)
{
    // All allocators that use reservations guarentee zeros
}

#endif

VIS_HIDDEN
void __attribute__((always_inline)) crt_init_new_globals(capability* segment_table, struct capreloc* start, struct capreloc* end) {
    crt_init_common(segment_table, start, end, RELOC_FLAGS_TLS);
}

VIS_HIDDEN
void crt_init_new_locals(capability* segment_table, struct capreloc* start, struct capreloc* end) {
    crt_init_common(segment_table, start, end, 0);
#ifndef IS_KERNEL
#ifndef IS_BOOT
    size_t ndx = get_tls_sym_captable_ndx16(thread_local_tls_seg);
    char* tls_seg = segment_table[crt_tls_seg_off/sizeof(capability)];
    ((capability*)(tls_seg + crt_cap_tab_local_addr))[ndx] = tls_seg;
#endif
#endif
}

VIS_HIDDEN
void __attribute__((always_inline)) crt_init_new_locals_inline(capability* segment_table, struct capreloc* start, struct capreloc* end) {
	crt_init_common(segment_table, start, end, 0);
}

// sorted in increasing order
uint8_t crt_sorted_table_map[MAX_SEGS];

VIS_HIDDEN
char* crt_logical_to_cap(size_t logical_address, size_t size, int tls, char* tls_seg) {

    static int map_sorted = 0;

    if(!map_sorted) {
        map_sorted = 1;


        for(size_t i = 0; i != MAX_SEGS; i++) {
            crt_sorted_table_map[i] = i;
        }

        size_t i = 1;

        // Sort increasing order, dont want to include quicksort in crt, and this is very small, so just bubble sort.
        while(i != MAX_SEGS) {
            if(crt_segment_table_vaddrs[crt_sorted_table_map[i]] <
                    crt_segment_table_vaddrs[crt_sorted_table_map[i-1]]) {
                uint8_t tmp = crt_sorted_table_map[i];
                crt_sorted_table_map[i] = crt_sorted_table_map[i-1];
                crt_sorted_table_map[i-1] = tmp;
                if(i != 1) i--;
            } else i++;
        }
    }

    uint8_t ndx = 0;

    while(logical_address >= crt_segment_table_vaddrs[crt_sorted_table_map[ndx+1]]) {
        ndx++;
    }

    size_t tls_ndx = crt_tls_seg_off / sizeof(capability);
    int current_is_tls = crt_sorted_table_map[ndx] == tls_ndx;
    // There may be two segments at this address because one is our TLS segment
    if(current_is_tls || (ndx != 0 && crt_sorted_table_map[ndx-1] == tls_ndx)) {
        if(!current_is_tls != !tls) ndx--;
    }

    ndx = crt_sorted_table_map[ndx];

    char* seg = ((ndx == tls_ndx) && tls_seg) ? tls_seg : (char*)(crt_segment_table[ndx]);

    size_t offset = logical_address - crt_segment_table_vaddrs[ndx];
    char* cap = (char*)cheri_setbounds(seg + offset, size);
    return cap;
}
