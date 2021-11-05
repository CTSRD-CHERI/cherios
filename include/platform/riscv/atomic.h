/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lawrence Esswood
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#ifndef CHERIOS_ATOMIC_H
#define CHERIOS_ATOMIC_H

#include "stdatomic.h"

// These atomics were created / used before stdatomics were supported. It would be nice to remove them completely.

#define ATOMIC_64 _Atomic(uint64_t)
#define ATOMIC_32 _Atomic(uint32_t)
#define ATOMIC_16 _Atomic(uint16_t)
#define ATOMIC_8 _Atomic(uint8_t)
#define ATOMIC_c _Atomic(void*)
#define ATOMIC_T(type) ATOMIC_ ## type

#define ATOMIC_ADD(pointer, type, val_type, val, result) result = (CTYPE(type))atomic_fetch_add((volatile ATOMIC_T(type)*)pointer, val)
#define ATOMIC_ADD_RV(pointer, type, val_type, val) (CTYPE(type))atomic_fetch_add((volatile ATOMIC_T(type)*)pointer, val)

#define ATOMIC_CAS(pointer, type, old_val, new_val, result) \
    do {CTYPE(type) _cas_tmp = old_val; result = atomic_compare_exchange_weak((volatile ATOMIC_T(type)*)pointer, &_cas_tmp, new_val);} while(0)
#define ATOMIC_CAS_RV(pointer, type, old_val, new_val) \
    ({CTYPE(type) _cas_tmp = old_val; atomic_compare_exchange_weak((volatile ATOMIC_T(type)*)pointer, &_cas_tmp, new_val);})

#define ATOMIC_SWAP(pointer, type, new_val, result) result = (CTYPE(type))atomic_exchange((volatile ATOMIC_T(type)*)pointer, new_val)
#define ATOMIC_SWAP_RV(pointer, type, new_val) (CTYPE(type))atomic_exchange((volatile ATOMIC_T(type)*)pointer, new_val)

// These are even worse because exposing LOAD_LINK / STORE_COND as seperate macros could easily lead to sequences that
// can never succeed. Only the kernel uses these in two places. They should be updated. Do not create any new uses.

#define LOAD_LINK(ptr, type, result) \
    __asm__ __volatile(LOADL(type) " %[res], 0(%[pt])" : [res] OUT(type) (result) : [pt] IN(c) (ptr):)

#define STORE_COND(ptr, type, val, suc) __asm__ __volatile(STOREC(type) " %[sc], %[vl], 0(%[pt])" : \
                    [sc] OUT(64) (suc) : \
                    [pt] IN(c) (ptr), [vl] IN(type) (val) : "memory")

#endif //CHERIOS_ATOMIC_H
