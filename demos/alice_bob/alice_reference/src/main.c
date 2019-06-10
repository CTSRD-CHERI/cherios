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

#include "cheric.h"
#include "bob_id.h"
#include "stdio.h"
#include "nano/foundations.h"
#include "namespace.h"
#include "alice_bob.h"
#include "assert.h"
#include "capmalloc.h"
#include "lorem.h"

static inline found_id_t* get_bob_id(void) {
    found_id_t* bob_id = namespace_get_found_id(namespace_id_num_bob);
    assert(found_id_metadata_equal(bob_id, &bob_elf_id));
    return bob_id;
}

static inline void print_bob(void) {
    printf("Alice thinks bob is:\n");
    print_id(&bob_elf_id);
}

// Send message insecure
static inline void send1(act_kt act) {
    char* response1 = (char*)message_send_c(0, 0, 0, 0, READ_ONLY(VERY_SECRET_DATA), NULL, NULL, NULL, act, SYNC_CALL, BOB_PORT_INSECURE);
    printf("Alice1: I got a response of %s\n", response1);
}

// Lock message
static inline void send2(act_kt act, found_id_t* id) {
    // Lock the message
    res_t lock_res = cap_malloc(RES_CERT_META_SIZE);
    locked_t locked = rescap_take_locked(lock_res, NULL, CHERI_PERM_LOAD, id, NULL, READ_ONLY(VERY_SECRET_DATA));

    char* response2 = (char*)message_send_c(0, 0, 0, 0, locked, NULL, NULL, NULL, act, SYNC_CALL, BOB_PORT_INSECURE);
    printf("Alice2: I got a response of %s\n", response2);
}

// Lock message, check signed response
static inline void send3(act_kt act, found_id_t* id) {
    // Lock the message
    res_t lock_res = cap_malloc(RES_CERT_META_SIZE);
    locked_t locked = rescap_take_locked(lock_res, NULL, CHERI_PERM_LOAD, id, NULL, READ_ONLY(VERY_SECRET_DATA));

    cert_t response3 = (cert_t*)message_send_c(0, 0, 0, 0, locked, NULL, NULL, NULL, act, SYNC_CALL, BOB_PORT_SIGNED);

    _safe cap_pair pair;

    found_id_t* response_id = rescap_check_cert(response3, &pair);

    assert(found_id_equal(id, response_id));

    printf("Alice3: I got a response of %s\n", (char*)pair.data);
}

// Lock a reference to a writable structure
static inline void send4(act_kt act, found_id_t* id) {
    res_t lock_res = cap_malloc(RES_CERT_META_SIZE + sizeof(by_ref_type));
    _safe cap_pair pair;

    locked_t locked = rescap_take_locked(lock_res, &pair, CHERI_PERM_ALL, id, NULL, NULL);

    by_ref_type* ref = (by_ref_type*)pair.data;

    ref->message = READ_ONLY(VERY_SECRET_DATA);

    message_send_c(0, 0, 0, 0, locked, NULL, NULL, NULL, act, SYNC_CALL, BOB_PORT_BY_REFERENCE);

    printf("Alice4: I got a response of %s\n", ref->response);
}

// Lock a reference to a writable structure that can async call back
static inline void send5(act_kt act, found_id_t* id) {
    res_t lock_res = cap_malloc(RES_CERT_META_SIZE + sizeof(by_ref_type2));
    _safe cap_pair pair;

    locked_t locked = rescap_take_locked(lock_res, &pair, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP, id, NULL, NULL);

    by_ref_type2* ref = (by_ref_type2*)pair.data;

    ref->message = READ_ONLY(VERY_SECRET_DATA);
    ref->nonce = cap_malloc(1);
    ref->reply_to = act_self_ref;

    message_send_c(0, 0, 0, 0, locked, NULL, NULL, NULL, act, SEND_SWITCH, BOB_PORT_ASYNC);

    msg_t* m = get_message();

    capability response = m->c3;
    capability nonce = m->c4;

    assert(nonce == ref->nonce);

    next_msg();

    printf("Alice5: I got a response of %s\n", (char*)response);
}

// Pass a reference to a socket requester, restrict for bob
static inline void send6(act_kt act, __unused found_id_t* id) {
    requester_t req = socket_malloc_requester_32(SOCK_TYPE_PUSH, NULL);

    socket_requester_restrict_auth(req, id, id);

    message_send_c(0, 0, 0, 0, socket_make_ref_for_fulfill(req), NULL, NULL, NULL, act, SYNC_CALL, BOB_PORT_SOCKET);

    socket_requester_connect(req);


    socket_requester_space_wait(req, 1, 0, 0);
    socket_request_ind(req, LOREM, 0x1000, 0);

    socket_requester_wait_all_finish(req, 0);

    socket_close_requester(req, 1, 0);
}

int main(__unused register_t arg, __unused capability carg) {

    act_kt eve = namespace_get_ref(namespace_num_eve);

    assert(eve);

    send1(eve);

    print_bob();
    found_id_t* id = get_bob_id();

    send2(eve, id);

    send3(eve, id);

    send4(eve, id);

    send5(eve, id);

    send6(eve, id);

    return 0;
}
