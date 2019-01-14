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

#include "deduplicate.h"
#include "namespace.h"
#include "msg.h"
#include "sha256.h"
#include "dylink.h"
#include "stdio.h"
#include "assert.h"
#include "string.h"
#include "nano/usernano.h"

static act_kt dedup_service = NULL;

act_kt get_dedup(void) {
    if(dedup_service == NULL) {
        dedup_service = namespace_get_ref(namespace_num_dedup_service);
    }
    return dedup_service;
}

ERROR_T(entry_t)    deduplicate(uint64_t* data, size_t length) {
    act_kt serv = get_dedup();
    return MAKE_VALID(entry_t,message_send_c(length, 0, 0, 0, data, NULL, NULL, NULL, serv, SYNC_CALL, 0));
}

ERROR_T(entry_t)    deduplicate_dont_create(uint64_t* data, size_t length) {
    act_kt serv = get_dedup();
    return MAKE_VALID(entry_t,message_send_c(length, 0, 0, 0, data, NULL, NULL, NULL, serv, SYNC_CALL, 2));
}

entry_t             deduplicate_find(sha256_hash hash) {
    act_kt serv = get_dedup();
    return (entry_t)message_send_c(hash.doublewords[0], hash.doublewords[1], hash.doublewords[2], hash.doublewords[3],
                                   NULL, NULL, NULL, NULL, serv, SYNC_CALL, 1);
}

typedef void func_t(void);

capability deduplicate_cap(capability cap, int allow_create) {

    if(get_dedup() == NULL) {
        return cap;
    }

    size_t offset = cheri_getoffset(cap);
    size_t length = cheri_getlen(cap);
    register_t perms = cheri_getperm(cap);
    register_t is_func = (perms & CHERI_PERM_EXECUTE);

    capability resolve = cheri_setoffset(cap, 0);

    ERROR_T(entry_t) result = (allow_create) ?
                              (deduplicate((uint64_t*)resolve, length)) :
                              (deduplicate_dont_create((uint64_t*)resolve, length));

    if(!IS_VALID(result)) {
        printf("Deduplication error: %d\n", (int)result.er);
        CHERI_PRINT_CAP(resolve);
        sleep(0x1000);
        assert(0);
        return cap;
    }

    entry_t entry = result.val;

    capability open = foundation_entry_expose(entry);

    assert(cheri_getlen(open) == length);

    int res = memcmp(resolve, open, length);

    assert(res == 0);

    open = cheri_andperm(cheri_setoffset(open, offset), perms);

    return open;
}



void deduplicate_all_withperms(int allow_create, register_t perms) {
    func_t ** cap_table_globals = (func_t **)get_cgp();
    size_t n_ents = cheri_getlen(cap_table_globals)/sizeof(func_t*);

    size_t processed = 0;
    size_t of_which_func = 0;
    size_t of_which_replaced = 0;

    for(size_t i = 0; i != n_ents; i++) {
        func_t * to_dedup = cap_table_globals[i];
        processed ++;

        register_t  has_perms = cheri_getperm(to_dedup);

        // Don't deduplicate if this is writable or if this doesn't have the permissions we want
        if((has_perms & perms) != perms ||
                (has_perms & (CHERI_PERM_STORE | CHERI_PERM_STORE_CAP)) ||
                cheri_getsealed(to_dedup)) {
            continue;
        }

        of_which_func++;

        func_t* res = (func_t*)deduplicate_cap((capability )to_dedup, allow_create);

        if(res != to_dedup) {
            of_which_replaced++;
            cap_table_globals[i] = res;
        }
    }

    printf("Ran deduplication on all. Processed %ld, Found %ld, Replaced %ld\n", processed, of_which_func, of_which_replaced);
}

void deduplicate_all_functions(int allow_create) {
    deduplicate_all_withperms(allow_create, CHERI_PERM_EXECUTE);
}