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
#include "stdlib.h"
#include "crt.h"

static act_kt dedup_service = NULL;

act_kt get_dedup(void) {
    if(dedup_service == NULL) {
        dedup_service = namespace_get_ref(namespace_num_dedup_service);
    }
    return dedup_service;
}

act_kt set_custom_dedup(act_kt dedup) {
    dedup_service = dedup;
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

capability deduplicate_cap(capability cap, int allow_create, register_t perms) {

    if(get_dedup() == NULL) {
        return cap;
    }

    size_t offset = cheri_getoffset(cap);
    size_t length = cheri_getlen(cap);
    register_t is_func = (perms & CHERI_PERM_EXECUTE);

    capability resolve = cheri_setoffset(cap, 0);

    ERROR_T(entry_t) result = (allow_create) ?
                              (deduplicate((uint64_t*)resolve, length)) :
                              (deduplicate_dont_create((uint64_t*)resolve, length));

    if(!IS_VALID(result)) {
        if(result.er == 0) return cap;
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



dedup_stats deduplicate_all_withperms(int allow_create, register_t perms) {
    func_t ** cap_table_globals = (func_t **)get_cgp();
    size_t n_ents = cheri_getlen(cap_table_globals)/sizeof(func_t*);

    dedup_stats stats;
    bzero(&stats, sizeof(dedup_stats));

#define ALIGN_COPY_SIZE 0x4000

    uint64_t buffer[ALIGN_COPY_SIZE/sizeof(uint64_t)];

    for(size_t i = 0; i != n_ents; i++) {
        func_t * to_dedup = cap_table_globals[i];
        stats.processed ++;

        register_t  has_perms = cheri_getperm(to_dedup);

        // Don't deduplicate if this is writable or if this doesn't have the permissions we want
        if(!to_dedup || (cheri_getlen(to_dedup) == 0) ||
                ((has_perms & perms) != perms) ||
                (has_perms & (CHERI_PERM_STORE | CHERI_PERM_STORE_CAP)) || // Also LOAD_CAP ? deduplicates have no tags
                cheri_getsealed(to_dedup)) {
            continue;
        }

        stats.tried++;

        int bad_align = (cheri_getoffset(to_dedup) != 0 ||
                (cheri_getlen(to_dedup) & 0x7) != 0 ||
                (cheri_getcursor(to_dedup) &0x7) != 0);
        int isfunc = ((((size_t)to_dedup) & 0x3) == 0 && cheri_getoffset(to_dedup) == 0 && (cheri_getlen(to_dedup) & 0x3) == 0);

        capability orig = to_dedup;
        // A bit dangerous if we ever have offsets that we actually need
        // This allows us to deduplicate things with bad alignment
        // We copy it into a buffer that aligns well and pad the ends with zeros
        size_t true_size = cheri_getlen(to_dedup) - cheri_getoffset(to_dedup);

        if(bad_align) {
            if(true_size > ALIGN_COPY_SIZE) {
                stats.too_large++;
                continue;
            }
            buffer[true_size/8] = 0; // Pad out the last few (up to 8) bytes with zeros
            memcpy(buffer, to_dedup, true_size); // Fill in the bytes to dedup
            to_dedup = cheri_setbounds_exact(buffer, (true_size+7) & ~7);
        }

        if(isfunc) {
            stats.of_which_func++;
            stats.of_which_func_bytes += true_size;
        }
        else {
            stats.of_which_data++;
            stats.of_which_data_bytes += true_size;
        }

        func_t* res = (func_t*)deduplicate_cap((capability )to_dedup, allow_create, has_perms);

        if(res != to_dedup) {
            res = cheri_setbounds_exact(res, true_size);
            if(isfunc) {
                stats.of_which_func_replaced++;
                stats.of_which_func_bytes_replaced += true_size;
            }
            else {
                stats.of_which_data_replaced++;
                stats.of_which_data_bytes_replaced += true_size;
            }

            cap_table_globals[i] = res;
        }
    }

    return stats;
}

// FIXME: Need to invalidate caches as this moves code around
// FIXME: Won't work (at present) if functions overlap
// NOTE: This function can ONLY be called from init.S. It will fix up only one return address
// NOTE: This function relocates all of the functions still inside of the code segment. This might very well include
// THIS function itsel.It is highly advised not to call any sub-routines when movement takes place
// NOTE: We try hard to make sure this function deduplicates, but if it doesn't, some juggling is required

capability* seg_tbl_for_cmp;

static int cap_tab_cmp(const void* a, const void* b) {

    struct capreloc* start = &__start___cap_relocs;
    struct capreloc* ra = &start[*((size_t*)a)];
    struct capreloc* rb = &start[*((size_t*)b)];

    capability va = *RELOC_GET_LOC(seg_tbl_for_cmp, ra);
    capability vb = *RELOC_GET_LOC(seg_tbl_for_cmp, rb);

    return (int)(va) - (int)(vb);
}

#define MAP_SIZE 0x1000 // How many relocations we are willing to re-parse
#define RET_SIZE 0x200  // The size of the function that called us. We currently take a copy because it makes things easy

// ret is the function we would return to if no compaction took place
// return value is the function we should instead return to (we in fact return to a trampoline that does this for us)

capability compact_code(capability* segment_table, struct capreloc* start, struct capreloc* end,
                        char* code_seg_write, size_t code_seg_offset, startup_flags_e flags, capability ret) {


// If we don't deduplicate there is no point trying to compact

#if (AUTO_DEDUP_ALL_FUNCTIONS && AUTO_COMPACT)

    // This is all a bit hacky, but I don't want to write this in assembly
    // Hopefully these run before we clobber c17?
    size_t here = (size_t)cheri_getreg(12); // better to use &compact_code?
    size_t called_from = (size_t)cheri_getreg(17);

    if((flags & (STARTUP_NO_COMPACT | STARTUP_NO_DEDUP)) || (dedup_service == NULL)) return ret;

    size_t code_seg_ndx = code_seg_offset / sizeof(capability);
    char* code_seg_exe = (char*)segment_table[code_seg_ndx];

    size_t need_to_move[MAP_SIZE];
    size_t n_to_move = 0;

    assert(cheri_getoffset(code_seg_write) == 0);
    assert((size_t)code_seg_write == (size_t)code_seg_exe);

    size_t code_bot = cheri_getbase(code_seg_write);
    size_t code_top = code_bot + cheri_getlen(code_seg_write);


    assert(!(code_bot <= here && here < code_top));
    assert(!(code_bot <= called_from && called_from < code_top));

    // build a map into the relocations for all capabilities that target the code segment


    size_t i = 0;
    for(struct capreloc* reloc = start;reloc != end; i++,reloc++) {
        uint8_t ob_seg_ndx = (uint8_t)(reloc->object_seg_ndx);

        if(reloc->flags & RELOC_FLAGS_TLS) continue;
        if(ob_seg_ndx != code_seg_ndx) continue; // We are only compacting the code seg
        if(reloc->size == 0) continue; // zero size things are likely to be information passed by the linker and should not be changed

        capability found = *RELOC_GET_LOC(segment_table, reloc);

        assert(cheri_gettag(found));

        size_t currently_points_to = (size_t)cheri_getbase(found);

        if(code_bot <= currently_points_to && currently_points_to < code_top) {
            assert(n_to_move != MAP_SIZE);
            assert(!(cheri_getperm(found) & CHERI_PERM_STORE));
            need_to_move[n_to_move++] = i;
        }
    }

    // sort (Subroutine is fine as we are not yet moving)
    seg_tbl_for_cmp = segment_table;
    qsort(need_to_move, n_to_move, sizeof(size_t), cap_tab_cmp);

    // now we have an ordered map into the cap table we have to perform compaction

    // Problems:
    //  We overwrite the current function (solved by assert that this function has already been deduplicated)
    //  We overwrite the return function (we save it first to the stack, then make a new return function)

    size_t ret_base = cheri_getbase(ret);

    int need_new_ret = (code_bot <= ret_base && ret_base < code_top);

    size_t ret_offset = cheri_getoffset(ret);
    size_t ret_len = cheri_getlen(ret);
    capability saved_ret[(RET_SIZE/sizeof(capability))+sizeof(capability)];

    if(need_new_ret) {

        assert_int_ex(ret_len, <=, RET_SIZE);
        memcpy(saved_ret, cheri_setoffset(ret, 0), ret_len);
    }

    // THIS IS THE DANGEROUS BIT

    size_t orig_size = cheri_getlen(code_seg_write);
    size_t new_size = 0;

    uint32_t* to = (uint32_t*)code_seg_write;

    capability last_found = NULL;
    capability last_compact = NULL;
    for(i = 0; i != n_to_move; i++) {

        struct capreloc* reloc = &start[need_to_move[i]];

        capability* loc = RELOC_GET_LOC(segment_table, reloc);
        capability found = *loc;

        // We might generate the same object at different locations. Only need to compact once.
        if((last_found == NULL) || cheri_getbase(found) >= cheri_gettop(last_found)) {

            assert(cheri_getoffset(found) == 0);

            size_t size = cheri_getlen(found);

            int mis_aligned = ((size_t)found | size) & 0x3;

            if(!mis_aligned) {
                // We are copying something that was aligned to a word. Keep the target the same alignment.

                size_t adjust_target = (-new_size) & 0x3;

                new_size += adjust_target;
                code_seg_exe+=adjust_target;
                to = (uint32_t*)(((char*)to) + adjust_target);

                uint32_t* from = (uint32_t*)found;
                for(size_t w = 0; w != size/4; w++) {
                    *(to++) = *(from++);
                }

            } else {
                // Copy byte by byte
                char* src = (char*)found;
                char* dst = (char*)to;
                to = (uint32_t*)(dst + size);
                while(dst != (char*)to) {
                    *(dst++) = *(src++);
                }

            }

            last_compact = cheri_setbounds_exact(code_seg_exe, size);
            last_found = found;
            new_size += size;
            code_seg_exe +=size;

            *loc = last_compact;

        } else if (found != last_found ){
            // what we found belongs to the last object
            size_t base_off = cheri_getbase(found) - cheri_getbase(last_found);
            size_t off = cheri_getoffset(found);
            size_t len = cheri_getlen(found);

            capability sub_ob = cheri_incoffset(cheri_setbounds_exact(cheri_incoffset(last_compact, base_off), len), off);

            *loc = sub_ob;
        } else {
            *loc = last_compact;
        }

    }

    // FIXME: Invalidate ICache

    // Danger over. Can call things again.

    // TODO: Sanity check this by zero-ing out the remaining space?

    if(need_new_ret) {
        memcpy(to, saved_ret, ret_len);
        new_size += ret_len;
        capability new_ret = cheri_setbounds_exact(code_seg_exe, ret_len);
        new_ret = cheri_setoffset(new_ret, ret_offset);

        ret = new_ret;
    }

#if (AUTO_DEDUP_STATS)
    printf("%s's code segment went from %ld bytes to %ld bytes\n", syscall_get_name(act_self_ref), orig_size, new_size);
#endif


#endif

    return ret;
}

dedup_stats deduplicate_all_functions(int allow_create) {
    return deduplicate_all_withperms(allow_create, CHERI_PERM_EXECUTE);
}