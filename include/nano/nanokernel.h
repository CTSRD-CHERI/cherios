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

#ifndef CHERIOS_NANOKERNEL_H
#define CHERIOS_NANOKERNEL_H

#include "nano_if_list.h"
#include "nanotypes.h"

#ifndef __ASSEMBLY__

#include "cdefs.h"
#include "cheric.h"
#include "cheriplt.h"
#include "types.h"
#include "math_utils.h"

__BEGIN_DECLS
PLT(nano_kernel_if_t, NANO_KERNEL_IF_LIST)
__END_DECLS

#define ALLOCATE_PLT_NANO PLT_ALLOCATE_csd(nano_kernel_if_t, NANO_KERNEL_IF_LIST)


extern capability int_cap;
#define MAKE_SEALABLE_INT(x) cheri_setoffset(int_cap, x)

#ifdef PLATFORM_riscv
typedef capability pad_t[0x10];
static inline entry_t foundation_create(res_t res, size_t image_size, capability image, size_t entry0, size_t n_entries, register_t is_public) {
    _safe pad_t pad;
    return foundation_create_need_pad(res, image_size, image, entry0, n_entries, is_public, (capability *)((char*)pad + sizeof(pad_t)));
}
#endif

/* Safe (assuming the nano kernel performs relevent locking n the presence of a race */
static inline void try_take_end_of_res(res_t res, size_t required, cap_pair* out) {
    out->data = out->code = NULL;
    size_t set_bound_to = required;
    required = align_up_to(set_bound_to, RES_META_SIZE);
    if(res != NULL) {
        res_nfo_t nfo = rescap_nfo(res);
        size_t length = nfo.length;

        if(length > required + RES_META_SIZE) {
            res = rescap_split(res, length - (required + RES_META_SIZE));
        }
        if(res != NULL && length >= required) {
            rescap_take(res, out);

            if(out->data != NULL) {
                if(cheri_getlen(out->data) < set_bound_to) {
                    out->data = out->code = NULL;
                } else {
                    out->data = cheri_setbounds(out->data, set_bound_to);
                    out->code = cheri_setbounds(out->code, set_bound_to);
                }
            }
        }

    }
}

static inline void try_take_res(res_t res, size_t required, cap_pair* out) {
    out->data = out->code = NULL;
    if(res != NULL && cheri_gettype(res) == RES_TYPE) {
        rescap_take(res, out);
        if(out->data != NULL && (cheri_getlen((out->data)) >= required)) {
            out->data = cheri_setbounds_exact(out->data, required);
            out->code = cheri_setbounds_exact(out->code, required);
        }
    }
}

/* Try to ask memgt instead of using this */
static inline capability get_phy_cap(page_t* book, size_t address, size_t size, int cached, int IO) {
    size_t phy_page = address / PAGE_SIZE;
    size_t phy_offset = address & (PAGE_SIZE - 1);
    size_t n_pages = 1 + ((phy_offset + size) / PAGE_SIZE);

    if(book[phy_page].len == 0) {
        size_t search_index = 0;
        while(book[search_index].len + search_index < phy_page) {
            search_index = search_index + book[search_index].len;
        }
        split_phy_page_range(search_index, phy_page - search_index);
    }

    if(book[phy_page].len != n_pages) {
        split_phy_page_range(phy_page, n_pages);
    }

    cap_pair pair;
    get_phy_page(phy_page, cached, n_pages, &pair, IO);
    capability cap_for_phy = pair.data;
    cap_for_phy = cheri_setoffset(cap_for_phy, phy_offset);
    cap_for_phy = cheri_setbounds(cap_for_phy, size);
    return cap_for_phy;
}

/* Ask type_manager instead of calling this */

static inline capability get_sealing_cap_from_nano(register_t type) {
    return tres_take(tres_get(type));
}

static inline res_t reservation_precision_align(res_t res, size_t length, size_t mask) {
    res_nfo_t nfo = rescap_nfo(res);
    size_t rounded_base = (nfo.base + mask) & ~mask;
    size_t skip = rounded_base - nfo.base;
    if(skip) {
        res = rescap_split(res, skip - RES_META_SIZE);
        nfo.base += skip;
        nfo.length -= skip;
    }

    if(nfo.length != length) {
        rescap_split(res, length);
    }

    return res;
}

// These are the scales / sizes used by the nanokernel for splitsub.

static inline size_t scale_to_size(size_t scale) {
    scale++;
    size_t low = (scale & 0b11);
    size_t hi = scale >> 2;

    low = hi ? low | 4 : low;

    size_t less1 = hi-1;

    size_t res = (hi ? (low << less1) : low);

    return res;
}


// Works as long as (LOG_2_LARGEST_SIZE-LOG_2_SMALLEST_SIZE) <= (34)

// Maps:

// 0001 -> 0
// 0010 -> 1
// 0011 -> 2
// 0100 -> 3
// 0101 -> 4
// 0110 -> 5
// 0111 -> 6
// 1000 -> 7
// 1010 -> 8
// 1100 -> 9
// 1110 -> 10
// etc

// WARN: Handle a size of zero yourself, this will blow up

#define LOG_2_SMALLEST_SIZE 0
#define LOG_2_LARGEST_SIZE 18
#define LOG_2_DIFF (LOG_2_LARGEST_SIZE - LOG_2_SMALLEST_SIZE)
#define RES_SIZE_TYPE uint32_t // combats a bug where the compiler doesn't know sltiu doesn't need trucating

static inline RES_SIZE_TYPE size_to_scale(RES_SIZE_TYPE size) {

// Benefits of offsetting:
//  Lets us use one fewer rounds if we near a boundary with largest size
//  all values need rounding, not just all but those that are exact sizes
//  gives an index starting at 0
        size --;

#if (LOG_2_SMALLEST_SIZE <= 3)
        if(size <= 8) return size;
#endif

        RES_SIZE_TYPE shifted = size >> (LOG_2_SMALLEST_SIZE + 2);
        RES_SIZE_TYPE log = 0;

#if ((LOG_2_LARGEST_SIZE - LOG_2_SMALLEST_SIZE) > 18)
RES_SIZE_TYPE b5 = (shifted & (0xFFFF0000)) ? 16 : 0;
    shifted >>= b5;
    log |= b5;
#endif
#if ((LOG_2_LARGEST_SIZE - LOG_2_SMALLEST_SIZE) > 10)
RES_SIZE_TYPE b4 = (shifted & (0xFF00)) ? 8 : 0;
    shifted >>= b4;
    log |= b4;
#endif
#if ((LOG_2_LARGEST_SIZE - LOG_2_SMALLEST_SIZE) > 6)
RES_SIZE_TYPE b3 = (shifted & (0xF0)) ? 4 : 0;
    shifted >>= b3;
    log |= b3;
#endif
#if ((LOG_2_LARGEST_SIZE - LOG_2_SMALLEST_SIZE) > 4)
RES_SIZE_TYPE b2 = (shifted & (0xC)) ? 2 : 0;
    shifted >>= b2;
    log |= b2;
#endif
        RES_SIZE_TYPE b1 = (shifted & (0x2)) ? 1 : 0;
        log |= b1;

        // log is the base 2 logarithm of (size >> (LOG_2_SMALLEST_SIZE + 2)), rounded down

        log += LOG_2_SMALLEST_SIZE;

        RES_SIZE_TYPE low_bits = (size >> log);

        low_bits &= 0b11;

        RES_SIZE_TYPE res = (low_bits | ((log + 1) << 2));

        return res;
}

#else

#define LOCAL_CAP_VAR_MACRO(item,...)   local_cap_var item ## _cap;
#define INIT_TABLE_MACRO(item,...)      init_table item;

#endif // __ASSEMBLY__

#endif //CHERIOS_NANOKERNEL_H
