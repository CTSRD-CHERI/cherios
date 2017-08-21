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

#include "cheric.h"
#include "nano/usernano.h"
#include "mman.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"

static void print_id(found_id_t* id) {
    printf("hash:\n");
    for(size_t i = 0; i < 32; i++) {
        printf("%02x", (int)(id->sha256[i] & 0xFF));
    }

    printf("\n");

    printf("entry: %lx. size:%lx. nent: %lx\n", id->e0, id->length, id->nentries);
}

static res_t get_res(size_t size) {
    // Mmmap new will use the message syscall.
    // All syscalls are in fact dangerous in secure code. Will write an enter/exit routine later.
    res_t res = mem_request(0, 0x1000, NONE, own_mop);

    if(res == NULL) {
        // Also dangerous
        printf(KRED"MMAP failed"KRST);
        exit(-1);
    }

    return res;
}

int main(register_t arg, capability carg) {

    init_nano_if_sys(); // <- this allows us to use non sys versions by calling syscall in advance for each function

    /* First try sign something */
    printf("Foundation test started.\n");

    res_t res1 = get_res(0x500);
    cap_pair pair1;
    cert_t certificate = rescap_take_cert(res1, &pair1, CHERI_PERM_LOAD);

    assert(pair1.data != NULL);
    assert(certificate != NULL);

#define MESSAGE "This message was signed! It can only have produced by me.\n"

    memcpy(pair1.data, MESSAGE, sizeof(MESSAGE));

    /* Now try check the signature */
    cap_pair pair2;
    found_id_t* id = rescap_check_cert(certificate, &pair2);

    CHERI_PRINT_CAP(pair1.code);
    CHERI_PRINT_CAP(pair1.data);
    CHERI_PRINT_CAP(pair2.code);
    CHERI_PRINT_CAP(pair2.data);

    print_id(id);

    printf("Signs message: %s", (char *)pair2.data);

    /* Now try to lock a message for ourselves */

    res_t res2 = get_res(0x500);
    cap_pair pair3;

    locked_t locked = rescap_take_locked(res2, &pair3, CHERI_PERM_LOAD, id);

    assert(pair3.data != NULL);
    assert(locked != NULL);

#define MESSAGE2 "This message is locked. It can only be opened by me.\n"
    memcpy(pair3.data, MESSAGE2, sizeof(MESSAGE2));

    /* Now try to unlock the message (while we are still us) */

    cap_pair pair4 = NULL_PAIR;

    rescap_unlock(locked, &pair4);

    CHERI_PRINT_CAP(pair3.code);
    CHERI_PRINT_CAP(pair3.data);
    CHERI_PRINT_CAP(pair4.code);
    CHERI_PRINT_CAP(pair4.data);

    assert(pair4.data != NULL);

    printf("Unlocked message: %s", (char*)pair4.data);

    /* Now exit the foundation. */
    foundation_exit();


    /* Now FAIL to unlock the message */

    cap_pair pair5 = NULL_PAIR;

    rescap_unlock(locked, &pair5);

    CHERI_PRINT_CAP(pair5.code);
    CHERI_PRINT_CAP(pair5.data);

    assert(pair5.data == NULL);

    printf("Foundation test finished!\n");
    return 0;
}
