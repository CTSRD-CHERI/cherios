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
    return dedup;
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

capability deduplicate_cap(capability cap, int allow_create, register_t perms, register_t length, register_t offset) {

    if(get_dedup() == NULL) {
        return cap;
    }

    //register_t is_func = (perms & CHERI_PERM_EXECUTE);

    capability resolve = cheri_incoffset(cap,-offset);

    ERROR_T(entry_t) result = (allow_create) ?
                              (deduplicate((uint64_t*)resolve, length)) :
                              (deduplicate_dont_create((uint64_t*)resolve, length));

    if(!IS_VALID(result)) {
        if(result.er == 0) return cap;
        printf("Deduplication error: %d. ac: %d. perms: %lx. len %lx. off %lx\n", (int)result.er, allow_create, perms, length, offset);
        CHERI_PRINT_CAP(resolve);
        sleep(0x1000);
        assert(0);
        return cap;
    }

    entry_t entry = result.val;

    capability open = foundation_entry_expose(entry);

    assert(cheri_getlen(open) >= length);

    open = cheri_incoffset(open, offset);

    int res = memcmp(resolve, open, length);

    assert(res == 0);

    open = cheri_andperm(open, perms);

    return open;
}

capability deduplicate_cap_precise(capability cap, int allow_create, register_t perms) {
    size_t offset = cheri_getoffset(cap);
    size_t length = cheri_getlen(cap);
    return deduplicate_cap(cap, allow_create, perms, length, offset);
}

capability deduplicate_cap_unaligned(capability cap, int allow_create, register_t perms) {
    capability bounce[0x100];

    capability precise = cap;

    if(cheri_getbase(cap) & 0x7) {
        assert(cheri_getlen(cap) <= sizeof(bounce));
        memcpy(bounce, cheri_setoffset(cap, 0), cheri_getlen(cap));
        precise = cheri_incoffset(cheri_setbounds_exact(bounce, cheri_getlen(cap)), cheri_getoffset(cap));
    }

    capability result = deduplicate_cap_precise(precise, allow_create, perms);

    return result == precise ? cap : result;
}

// TODO we should do a similar thing as compact and detect when relocations target the same object and re-derive

dedup_stats deduplicate_all_target(int allow_create, size_t ndx, capability* segment_table, struct capreloc* start, struct capreloc* end) {

    dedup_stats stats;
    bzero(&stats, sizeof(dedup_stats));

#define ALIGN_COPY_SIZE 0x4000

    uint64_t buffer[ALIGN_COPY_SIZE/sizeof(uint64_t)];

    for(struct capreloc* reloc = start; reloc != end; reloc++) {

        stats.processed ++;

        if(reloc->object_seg_ndx != ndx || reloc->size == 0) continue;

        capability* loc = RELOC_GET_LOC(segment_table, reloc);
        capability to_dedup = *loc;

        if(to_dedup == NULL || cheri_getsealed(to_dedup)) continue;

        register_t  has_perms = cheri_getperm(to_dedup);

        assert((has_perms & (CHERI_PERM_STORE | CHERI_PERM_STORE_CAP)) == 0);

        stats.tried++;

        uint64_t ob_size = reloc->size;
        uint64_t ob_off = reloc->offset;

        int bad_align = ((ob_size & 0x7) != 0) ||
                (((cheri_getcursor(to_dedup)-ob_off) &0x7) != 0);
        int isfunc = ((((size_t)to_dedup) & 0x3) == 0) && (ob_off == 0) && ((ob_size & 0x3) == 0);

        uint64_t size_to_dedup = ob_size;

        if(bad_align) {
            if(ob_size > ALIGN_COPY_SIZE) {
                stats.too_large++;
                continue;
            }
            buffer[ob_size/8] = 0; // Pad out the last few (up to 8) bytes with zeros
            memcpy(buffer, cheri_incoffset(to_dedup, -ob_off), ob_size); // Fill in the bytes to dedup
            size_to_dedup = (ob_size+7) & ~7;
            to_dedup = cheri_incoffset(cheri_setbounds(buffer, size_to_dedup), ob_off);
        }

        if(isfunc) {
            stats.of_which_func++;
            stats.of_which_func_bytes += ob_size;
        }
        else {
            stats.of_which_data++;
            stats.of_which_data_bytes += ob_size;
        }

        capability res = deduplicate_cap((capability)to_dedup, allow_create, has_perms, size_to_dedup, ob_off);

        if(res != to_dedup) {
            res = cheri_setbounds(res, ob_size);
            if(isfunc) {
                stats.of_which_func_replaced++;
                stats.of_which_func_bytes_replaced += ob_size;
            }
            else {
                stats.of_which_data_replaced++;
                stats.of_which_data_bytes_replaced += ob_size;
            }

            *loc = res;
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

#if (AUTO_DEDUP_ALL_FUNCTIONS && AUTO_COMPACT)

capability* seg_tbl_for_cmp;

static inline int reloc_cmp(const void* a, const void* b) {

    struct capreloc* start = &__start___cap_relocs;
    struct capreloc* ra = &start[*((const size_t*)a)];
    struct capreloc* rb = &start[*((const size_t*)b)];

    int diff = (int)ra->object - (int)rb->object; // lower address first

    return diff == 0 ? (int)(rb->size - ra->size) : diff; // larger size first
}

#endif

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

        size_t currently_points_to = (size_t)(found);

        if(code_bot <= currently_points_to && currently_points_to < code_top) {
            assert(n_to_move != MAP_SIZE);
            assert(!(cheri_getperm(found) & CHERI_PERM_STORE));
            need_to_move[n_to_move++] = i;
        }
    }

    // sort (Subroutine is fine as we are not yet moving)
    seg_tbl_for_cmp = segment_table;
    qsort(need_to_move, n_to_move, sizeof(size_t), reloc_cmp);

    // now we have an ordered map into the cap table we have to perform compaction

    // Problems:
    //  We overwrite the current function (solved by assert that this function has already been deduplicated)
    //  We overwrite the return function (we save it first to the stack, then make a new return function)
    //  We will move our cap relocs. When we notice doing this, update the start / end / loop variable appropriately

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

    uint64_t last_loc, last_size;

    // TODO: Currently this compact allows overlap (as this is the same behavior as the linker)
    // TODO: We coould also have a mode where we pad to stop aliasing

    // L2 is shared, so just invalidate L1

    for(uint64_t j = 0; j != CACHE_L1_INDEXS; j++) {
        CACHE_OP(CACHE_OP_INDEX_INVAL(CACHE_L1_INST), 0, j);
    }

    capability last_compact = NULL;
    for(i = 0; i != n_to_move; i++) {

        struct capreloc* reloc = &start[need_to_move[i]];

        capability* loc = RELOC_GET_LOC(segment_table, reloc);
        capability found = *loc;

        uint64_t ob_loc = reloc->object;
        uint64_t ob_off = reloc->offset;
        uint64_t ob_size = reloc->size;

        capability new_object;
        // We might generate the same object at different locations. Only need to compact once.
        if((last_compact == NULL) || ob_loc >= (last_loc+last_size)) {

            size_t mis_aligned = ((size_t)found | ob_size) & 0x3;

            if(!mis_aligned) {
                // We are copying something that was aligned to a word. Keep the target the same alignment.

                size_t adjust_target = (size_t)(-new_size) & 0x3;

                new_size += adjust_target;
                code_seg_exe+=adjust_target;
                to = (uint32_t*)(((char*)to) + adjust_target);

                uint32_t* from = (uint32_t*)((char*)found - ob_off);
                for(size_t w = 0; w != ob_size/4; w++) {
                    *(to++) = *(from++);
                }

            } else {
                // Copy byte by byte
                char* src = ((char*)found) - ob_off;
                char* dst = (char*)to;
                to = (uint32_t*)(dst + ob_size);
                while(dst != (char*)to) {
                    *(dst++) = *(src++);
                }

            }

            last_compact = cheri_setbounds(code_seg_exe, ob_size);
            last_size = ob_size;
            last_loc = ob_loc;

            new_size += ob_size;
            code_seg_exe +=ob_size;

            new_object = cheri_incoffset(last_compact, ob_off);

        } else {
            // what we found can be derived from the last compact

            assert(last_loc + last_size >= ob_loc + ob_size);

            size_t base_off = ob_loc - last_loc;

            capability sub_ob = cheri_incoffset(cheri_setbounds(cheri_incoffset(last_compact, base_off), ob_size), ob_off);

            new_object = sub_ob;
        }

        *loc = new_object;
        // We just moved the relocation table.
        if ((size_t)found == (size_t)start) {
            size_t tbl_size = (size_t)(end - start);
            start = new_object;
            end = start + tbl_size;
        }

    }

    for(uint64_t j = 0; j != CACHE_L1_INDEXS; j++) {
        CACHE_OP(CACHE_OP_INDEX_INVAL(CACHE_L1_INST), 0, j);
    }

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

#else
    (void)segment_table;
    (void)start;
    (void)end;
    (void)code_seg_write;
    (void)code_seg_offset;
    (void)flags;
#endif

    return ret;
}

dedup_stats deduplicate_all_functions(int allow_create) {
    size_t exe_ndx = (crt_code_seg_offset) / sizeof(capability);
    struct capreloc* start = &__start___cap_relocs;
    struct capreloc* end = (struct capreloc*)cheri_incoffset(start, cap_relocs_size);
    return deduplicate_all_target(allow_create, exe_ndx, crt_segment_table, start, end);
}