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

#include "cheric.h"
#include "cheriplt.h"
#include "types.h"
#include "math.h"

PLT(nano_kernel_if_t, NANO_KERNEL_IF_LIST)

#define ALLOCATE_PLT_NANO PLT_ALLOCATE_csd(nano_kernel_if_t, NANO_KERNEL_IF_LIST)


extern capability int_cap;
#define MAKE_SEALABLE_INT(x) cheri_setoffset(int_cap, x)

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

    if((phy_offset + size) > PAGE_SIZE) {
        /* If you want a better version use mmap */
        return NULL;
    }

    if(book[phy_page].len == 0) {
        size_t search_index = 0;
        while(book[search_index].len + search_index < phy_page) {
            search_index = search_index + book[search_index].len;
        }
        split_phy_page_range(search_index, phy_page - search_index);
    }

    if(book[phy_page].len != 1) {
        split_phy_page_range(phy_page, 1);
    }

    cap_pair pair;
    get_phy_page(phy_page, cached, 1, &pair, IO);
    capability cap_for_phy = pair.data;
    cap_for_phy = cheri_setoffset(cap_for_phy, phy_offset);
    cap_for_phy = cheri_setbounds(cap_for_phy, size);
    return cap_for_phy;
}

/* Ask type_manager instead of calling this */

static capability get_sealing_cap_from_nano(register_t type) {
    return tres_take(tres_get(type));
}

static res_t reservation_precision_align(res_t res, size_t length, size_t mask) {
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

#else

#define LOCAL_CAP_VAR_MACRO(item,...)   local_cap_var item ## _cap;
#define INIT_TABLE_MACRO(item,...)      init_table item;

#endif // __ASSEMBLY__

#endif //CHERIOS_NANOKERNEL_H
