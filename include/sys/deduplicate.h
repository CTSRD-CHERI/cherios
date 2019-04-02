/*-
 * Copyright (c) 2018 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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
#ifndef CHERIOS_DEDUPLICATE_H_H
#define CHERIOS_DEDUPLICATE_H_H

#include "cheric.h"
#include "nano/nanotypes.h"
#include "sha256.h"
#include "types.h"

#define DEDUP_ERROR_LENGTH_NOT_EVEN (-1)
#define DEDUP_ERROR_BAD_ALIGNMENT   (-2)

DEC_ERROR_T(entry_t);

ERROR_T(entry_t)    deduplicate(uint64_t* data, size_t length);
ERROR_T(entry_t)    deduplicate_dont_create(uint64_t* data, size_t length);
entry_t             deduplicate_find(sha256_hash hash);

// Tries to deduplicate every function. Bit extreme.

typedef struct {
    size_t processed;
    size_t tried;
    size_t of_which_func;
    size_t of_which_func_replaced;
    size_t of_which_data;
    size_t of_which_data_replaced;
    size_t too_large;
    size_t of_which_func_bytes;
    size_t of_which_func_bytes_replaced;
    size_t of_which_data_bytes;
    size_t of_which_data_bytes_replaced;
} dedup_stats;

dedup_stats deduplicate_all_functions(int allow_create);
capability deduplicate_cap_precise(capability cap, int allow_create, register_t perms);
capability deduplicate_cap(capability cap, int allow_create, register_t perms, register_t length, register_t offset);
capability deduplicate_cap_unaligned(capability cap, int allow_create, register_t perms);
act_kt get_dedup(void);
act_kt set_custom_dedup(act_kt dedup);

#define DEDUPLICATE_CAPCALL(F, allow)                                                           \
{                                                                                               \
    capability to_replace;                                                                      \
    __asm__ ("clcbi %[to_rep], %%capcall20(" #F ")($c25)\n" : [to_rep]"=C"(to_replace) ::);   \
    capability res = deduplicate_cap(to_replace, allow);                                        \
    __asm__ ("cscbi %[res], %%capcall20(" #F ")($c25)\n" : [res]"=C"(res) ::);                    \
}

#endif //CHERIOS_DEDUPLICATE_H_H
