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
    #include "assert.h"
    #include "misc.h"
    #include "object.h"
    #include "stdio.h"
    #include "namespace.h"
    #include "deduplicate.h"
    #include "string.h"
    #include "thread.h"
}


template <size_t size, size_t max_cuckoos, class mapT, mapT empty>
class cuckoomap {
    // STL was giving me pain - so here is a simple cuckoo hashmap from sha256 to an arbitrary type

public:
    struct map_entry {
        sha256_hash hash;
        mapT val;
    };

    map_entry* find_or_insert(sha256_hash hash, mapT& out_lost) {
        size_t h1 = hash1(hash);
        size_t h2 = hash2(hash);

        out_lost = nullptr;

        if(chash(table[h1].hash, hash)) return &table[h1];
        if(chash(table[h2].hash, hash)) return &table[h2];

        if(table[h1].val == empty) {
            table[h1].hash = hash;
            return &table[h1];
        } else if(table[h2].val == empty) {
            table[h2].hash = hash;
            return &table[h2];
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
                }

            }  while(kickedV != empty && cuckoos != max_cuckoos && index != h2);

            if(kickedV != empty) {
                out_lost = kickedV;
            }

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
        } else return nullptr;
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

cuckoomap<0x2000,0x10,entry_t, nullptr> map;

void hash_test(void) {
    // Check hashing is working...
    uint64_t aligned[64];
    sha256_hash hash;

#define TEST_STR(len, str, W1, W2, W3, W4)  \
    memcpy(aligned,str, len);               \
    sha256(len, aligned, &hash);                \
    assert_int_ex(hash.doublewords[0], ==, W1); \
    assert_int_ex(hash.doublewords[1], ==, W2); \
    assert_int_ex(hash.doublewords[2], ==, W3); \
    assert_int_ex(hash.doublewords[3], ==, W4)


    TEST_STR(56, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
             0x248D6A61D20638B8,
             0xE5C026930C3E6039,
             0xA33CE45964FF2167,
             0xF6ECEDD419DB06C1
    );

    TEST_STR(0, "",
             0xe3b0c44298fc1c14,
             0x9afbf4c8996fb924,
             0x27ae41e4649b934c,
             0xa495991b7852b855
    );

    TEST_STR(0x10, "Hello World!----",
             0x269e4f72239aee77,
             0xa468234fe51600c1,
             0x75690beee5d8a699,
             0x460365f37df5b9a3
    );
}

entry_t create(uint64_t* data, size_t length) {

    sha256_hash hash;
    sha256(length, data, &hash);

    entry_t lost = nullptr;

    auto result = map.find_or_insert(hash, lost);

    if(lost != nullptr) {
        // TODO: Got pushed out of the cache, fix this.
        assert(lost == nullptr && "Table overflow");
    }

    if(result->val != nullptr) return result->val;

    res_t res = cap_malloc(FOUNDATION_META_SIZE(1, length) + length);

    entry_t new_entry = foundation_create(res, length, data, 0, 1, 1);

    assert(new_entry != nullptr);

    result->val = new_entry;

    return new_entry;
}

entry_t find(sha256_hash hash) {
    auto result = map.find(hash);
    if(result == nullptr) return nullptr;
    else return result->val;

    return NULL;
}

entry_t dont_create(uint64_t* data, size_t length) {

    sha256_hash hash;
    sha256(length, data, &hash);

    return find(hash);
}


extern "C" {

    int be_public(void) {
        printf("Dedup going public\n");
        namespace_register(namespace_num_dedup_service, act_self_ref);
        return 0;
    }

    int main(__unused register_t arg, __unused capability carg) {
        printf("Deduplicate Hello World!\n");

        hash_test();

        msg_enable = 1;

        return 0;
    }

    ERROR_T(entry_t) __deduplicate(uint64_t* data, size_t length) {
        if((length & 0x7) != 0) return MAKE_ER(entry_t, DEDUP_ERROR_LENGTH_NOT_EVEN);

        if((cheri_getcursor(data) & 0x7) != 0) return MAKE_ER(entry_t, DEDUP_ERROR_BAD_ALIGNMENT);

        entry_t  res = create(data, length);

        return MAKE_VALID(entry_t, res);
    }

    ERROR_T(entry_t) __deduplicate_dont_create(uint64_t* data, size_t length) {
        if((length & 0x7) != 0) return MAKE_ER(entry_t, DEDUP_ERROR_LENGTH_NOT_EVEN);
        if((cheri_getcursor(data) & 0x7) != 0) return MAKE_ER(entry_t, DEDUP_ERROR_BAD_ALIGNMENT);
        return MAKE_VALID(entry_t, dont_create(data, length));
    }

    entry_t __deduplicate_find(struct sha256_hash hash) {
        return find(hash);
    }

    void (*msg_methods[]) = {(void*)&__deduplicate, (void*)&__deduplicate_find, (void*)&__deduplicate_dont_create, (void*)be_public};
    size_t msg_methods_nb = countof(msg_methods);
    void (*ctrl_methods[]) = {NULL};
    size_t ctrl_methods_nb = countof(ctrl_methods);
};

