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

#ifndef CHERIOS_CAPMALLOC_H
#define CHERIOS_CAPMALLOC_H

#include "cdefs.h"
#include "cheric.h"
#include "nano/nanokernel.h"

struct arena_t;


__BEGIN_DECLS
/* Get a reservation capability of size `size'. Will result in a claim being made on all memory the reservation covers,
 * both the metadata and the capability that would result from a take. */
VIS_EXTERNAL
res_t       cap_malloc(size_t size);

res_t cap_malloc_arena(size_t size, struct arena_t* arena);
res_t cap_malloc_arena_dma(size_t size, struct arena_t* arena, size_t* dma_off);

struct arena_t* new_arena(int dma);

/* Lay claim to a capability OR reservation. This capability will not be unmapped until free is called (by you). If cap
 * came from cap_malloc it will also be temporally safe. You may call claim multiple times. */
int         cap_claim(capability mem);

/* Free a capability. Afterwards use of this capability (by you) will be:
 * either an exception or unchanged data if cap was a result of cap_malloc, or
 * undefined if cap was not.
 * Note: If this came from cap_malloc, you may free EITHER the reservation or the resultant capability.
 * If you claim something else, you must free exactly that. */
void        cap_free(capability mem);

/* Note on free and claim: You may claim something X times. It will only be freed after calling free X times. */

void        init_cap_malloc(void);

__END_DECLS

#endif //CHERIOS_CAPMALLOC_H
