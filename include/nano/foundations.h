/*-
 * Copyright (c) 2019 Lawrence Esswood
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
#ifndef CHERIOS_FOUNDATIONS_H
#define CHERIOS_FOUNDATIONS_H

#include "nanotypes.h"
#include "stdio.h"

static inline void print_id(const found_id_t* id) {
    printf("hash:");
    for(size_t i = 0; i < 32; i++) {
        printf("%02x", (int)(id->sha256[i] & 0xFF));
    }

    printf("\n");

    printf("entry: %lx. size:%lx. nent: %lx\n", id->e0, id->length, id->nentries);
}

static inline int found_id_metadata_equal(const found_id_t* id1, const found_id_t* id2) {
    return memcmp((const char*)id1, (const char*)id2, sizeof(found_id_t)) == 0;
}

static inline int found_id_equal(const found_id_t* id1, const found_id_t* id2) {
    return id1 == id2;
}

auth_types_t get_authed_typed(capability cap) {
    uint64_t type = cheri_gettype(cap);

    switch(type) {
        case AUTH_PUBLIC_LOCKED:
        case AUTH_CERT:
        case AUTH_SINGLE_USE_CERT:
        case AUTH_SYMETRIC:
        case AUTH_INVOCABLE:
            return (auth_types_t)type;
        default:
            return AUTH_INVALID;
    }
}

#endif //CHERIOS_FOUNDATIONS_H
