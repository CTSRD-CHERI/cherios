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

#include "object.h"
#include "cheric.h"
#include "syscalls.h"
#include "namespace.h"
#include "assert.h"
#include "mman.h"
#include "stdio.h"

#define REVOKE_REG_NUM 13



#define DEFAULT_FORCE_REVOKE_SIZE (0x400000)

#define REVOKE_SIZE_START (0x400000)
#define REVOKE_SIZE_N 16

#define REVOKE_PHY_N 16
#define USAGE_INCREASE  (0x2000000)

#define S1              (0x0ff8000)
#define S2              (0x0007fc0)

typedef struct result_s {
    uint64_t phy_scanned;
    uint64_t virt_size;
    uint64_t time;
} result_t;


capability* blocks[REVOKE_PHY_N * 3];
size_t in = 0;

void increase_usage() {
    cap_pair p;


    size_t s = USAGE_INCREASE;

    precision_rounded_length pr;

    pr.mask = 0;

    pr = round_cheri_length(s);

    // actually allocates a bit too much, but hey
    ERROR_T(res_t) res = mem_request(0, pr.length + pr.mask, COMMIT_NOW, own_mop);

    res_t reser = reservation_precision_align(res.val, pr.length, pr.mask);

    rescap_take(reser, &p);

    blocks[in++] = (capability *)p.data;
}



void fill(capability cap) {
    for(size_t i = 0; i != in; i++) {
        capability* block = blocks[i];
        size_t n = cheri_getlen(block) / sizeof(capability);
        for(size_t j = 0; j != n; j++) {
            block[j] = cap;
        }
    }
}


enum type {
    NORMAL,
    ANY_CAP,
    REV_CAP
};

capability cause_revoke(result_t* result, size_t size, enum type e) {
    assert(msg_queue_empty());

    res_t res = mem_request(0, size-RES_META_SIZE, 0, own_mop).val;
    assert(cheri_gettag(res));
    res_nfo_t nfo = rescap_nfo(res);

    capability x = e == NORMAL ? NULL : (e == ANY_CAP ? (capability)&cause_revoke : (capability)res);
    fill(x);

    __unused int er = mem_release(nfo.base, nfo.length, 1, own_mop);

    assert(er == 0);

    msg_t* msg = get_message();
    result->phy_scanned = msg->a0;
    result->virt_size = msg->a1;
    result->time = msg->a2;

    next_msg();

    return (capability)res;
}

void print(result_t* r) {
    printf("0x%lx,0x%lx,0x%lx\n", r->phy_scanned, r->virt_size, r->time);
}

int main(__unused register_t arg, __unused capability carg) {
    act_kt revoke_act = namespace_get_ref(namespace_num_revoke_bench);

    assert(revoke_act != NULL);

    // Set self up as revoke thingy
    message_send(0, 0, 0, 0, act_self_ref, NULL, NULL, NULL, revoke_act, SYNC_CALL, REVOKE_REG_NUM);

    printf("Small pause...\n");

    sleep(MS_TO_CLOCK(1000));

    while(!msg_queue_empty()) {
        printf("Skipping...\n");
        while(!msg_queue_empty()) {
            next_msg();
        }
        printf("Small pause...\n");
        sleep(MS_TO_CLOCK(1000));
    }

    printf("Large pause...\n");
    sleep(MS_TO_CLOCK(3000));

    // At this point the system should have settled down enough to benchmark

    result_t results[REVOKE_SIZE_N*3];

    // Warm up
    cause_revoke(results, (size_t)((size_t)REVOKE_SIZE_START << (size_t)REVOKE_SIZE_N), NORMAL);


    // First we benchmark the effects of changing vmem size
    size_t size = REVOKE_SIZE_START;

    for(size_t i = 0; i != REVOKE_SIZE_N; i++) {
        cause_revoke(results+i, size, NORMAL);
        print(results+i);
        size <<= 1;
    }

    // Then of pmem size

    printf("Now for pmem\n");

    sleep(MS_TO_CLOCK(1000));

    for(size_t i = 0; i != REVOKE_PHY_N; i++) {
        cause_revoke(results + (3 * i), DEFAULT_FORCE_REVOKE_SIZE, NORMAL);
        print(results + (3 * i));
        cause_revoke(results + (3 * i) + 1, DEFAULT_FORCE_REVOKE_SIZE, ANY_CAP);
        print(results + (3 * i) + 1);
        cause_revoke(results + (3 * i) + 2, DEFAULT_FORCE_REVOKE_SIZE, REV_CAP);
        print(results + (3 * i) + 2);

        increase_usage();
    }

    for(size_t i = 0; i != (REVOKE_PHY_N*3); i++) {

    }

    return 0;
}
