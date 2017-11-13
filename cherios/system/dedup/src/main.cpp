/*-
 * Copyright (c) 2017 Lawrence Esswood
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

extern "C" {
    #include "cheric.h"
    #include "nano/nanotypes.h"
    #include "sha256.h"
    #include "capmalloc.h"
}


template <size_t size, size_t max_cuckoos, class mapT, mapT empty>
class cuckoomap {
    // STL was giving me pain - so here is a simple cuckoo hashmap from sha256 to an arbitrary type

public:
    struct map_entry {
        sha256_hash hash;
        mapT val;
    };

    map_entry* find_or_insert(sha256_hash hash, mapT &out_lost) {
        size_t h1 = hash1(hash);
        size_t h2 = hash2(hash);

        size_t knocks = 0;

        if(chash(table[h1].hash, hash)) return &table[h1];
        if(chash(table[h2].hash, hash)) return &table[h2];

        if(table[h1].val == empty) {
            table[h1].hash = hash;
            return &table[h1];
        } else {
            size_t cuckoos = 0;
            size_t index = h2;
            mapT value = empty;

            mapT kickedV;

            do {
                kickedV = table[index].val;
                sha256_hash kickedH = table[index].hash;

                table[index].hash = hash;
                table[index].val = value;

                if(kickedV != empty) {
                    size_t kh1 = hash1(kickedH);
                    size_t kh2 = hash2(kickedH);
                    size_t otherh = (index == kh1) ? kh2 : kh1;

                    hash = kickedH;
                    value = kickedV;
                    index = otherh;

                    cuckoos++;
                    if(cuckoos == max_cuckoos) {
                        out_lost = value;
                    }
                }


            }  while(kickedV != empty && cuckoos != max_cuckoos);

            return &table[h2];
        }
    }

    map_entry* find(sha256_hash hash) {
        size_t h1 = (hash.doublewords[0] ^ hash.doublewords[1]) % size;
        size_t h2 = (hash.doublewords[2] ^ hash.doublewords[3]) % size;

        if(chash(table[h1].hash,hash)) {
            return &(table[h1]);
        } else if (chash(table[h2].hash,hash)) {
            return &(table[h2]);
        } else return NULL;
    }

    void remove(map_entry* entry) {
        entry->val = empty;
        entry->hash.doublewords[0] = 0;
        entry->hash.doublewords[1] = 0;
        entry->hash.doublewords[2] = 0;
        entry->hash.doublewords[3] = 0;

    }

private:
    map_entry table[size];

    size_t hash1(sha256_hash hash) {
        return (hash.doublewords[0] ^ hash.doublewords[1]) % size;
    }

    size_t hash2(sha256_hash hash) {
        return (hash.doublewords[2] ^ hash.doublewords[3]) % size;
    }

    bool chash(sha256_hash h1, sha256_hash h2) {
        return (h1.doublewords[0] == h2.doublewords[0]) &&
                (h1.doublewords[1] == h2.doublewords[1]) &&
                (h1.doublewords[2] == h2.doublewords[2]) &&
                (h1.doublewords[3] == h2.doublewords[3]);
    }

};

cuckoomap<0x100,0x10,entry_t, nullptr> map;
 */

entry_t deduplicate(uint64_t* data, size_t length) {
    /*
    sha256_hash hash;
    sha256(length, data, &hash);

    entry_t lost = nullptr;

    auto result = map.find_or_insert(hash, lost);

    if(lost != nullptr) {
        // TODO: Got pushed out of the cache, fix this.
    }

    if(result->val != nullptr) return result->val;

    res_t res = cap_malloc(FOUNDATION_META_SIZE(1) + length);
    entry_t new_entry = foundation_create(res, length, data, 0, 1, 1);

    result->val = new_entry;

    return new_entry;
     */
    return NULL;
}

entry_t find(sha256_hash hash) {
    /*
    auto result = map.find(hash);
    if(result == nullptr) return nullptr;
    else return result->val;
     */
    return NULL;
}

#define DEDUP_ERROR_LENGTH_NOT_EVEN (-1)
#define DEDUP_ERROR_BAD_ALIGNMENT   (-2)



extern "C" {
    int main(register_t arg, capability carg) {

    }

    ERROR_T(entry_t) __deduplicate(uint64_t* data, size_t length) {
        if(length%2 != 0) return MAKE_ER(entry_t, DEDUP_ERROR_LENGTH_NOT_EVEN);
        if(cheri_getcursor(data) & 0x7 != 0) return MAKE_ER(entry_t, DEDUP_ERROR_BAD_ALIGNMENT);
        return MAKE_VALID(entry_t, deduplicate(data, length));
    }

    entry_t __deduplicate_find(struct sha256_hash hash) {
        return find(hash);
    }
};

