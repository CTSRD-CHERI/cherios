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
#include "nano/foundations.h"
#include "namespace.h"
#include "idnamespace.h"
#include "alice_bob.h"
#include "capmalloc.h"
#include "assert.h"
#include "misc.h"

cert_t response_cert;


capability maybe_unlock(capability input) {


    if(get_authed_typed(input) == AUTH_PUBLIC_LOCKED) {
        locked_t locked = (locked_t)input;
        _safe cap_pair pair;
        pair.data = NULL;
        rescap_unlock((auth_result_t)locked, &pair, own_auth, FOUND_LOCKED_TYPE);
        return pair.data;
    } else if(cheri_getsealed(input)) return NULL;
    else return input;

}

static inline capability handle(capability input, capability reponse) {
    char* unlocked = maybe_unlock(input);

    if(unlocked == NULL) {
        printf(KYLW"Bob: I don't understand what I just got sent..."KRST"\n");
    } else {
        int is_secret_message = memcmp(unlocked, VERY_SECRET_DATA, sizeof(VERY_SECRET_DATA)) == 0;
        printf(KYLW"Bob: I see you sent [%s]. Secret message? %s"KRST"\n", unlocked, is_secret_message ? "yes!" : "no.");
        if(is_secret_message) return reponse;
    }

    return NULL;
}

capability response1(capability input) {
    return handle(input, READ_ONLY(PROPER_RESPONSE));
}

capability response2(capability input) {
    return handle(input, (capability)response_cert);
}

capability response3(capability input) {
    by_ref_type* by_ref = (by_ref_type*)maybe_unlock(input);
    by_ref->response = handle(by_ref->message, READ_ONLY(PROPER_RESPONSE));
    return NULL;
}

void response4(capability input) {
    by_ref_type2* by_ref = (by_ref_type2*)maybe_unlock(input);
    capability response = handle(by_ref->message, READ_ONLY(PROPER_RESPONSE));

    message_send(0, 0 ,0, 0, response, by_ref->nonce, NULL, NULL, by_ref->reply_to, SEND_SWITCH, 0);
}


ssize_t CROSS_DOMAIN(ff)(capability arg, char* buf, uint64_t offset, uint64_t length);
__used ssize_t ff(__unused capability arg, char* buf, __unused uint64_t offset, uint64_t length) {
    printf(KYLW"Bob: Got some data: %*.*s"KRST"\n", (int)length, (int)length, buf);
    return length;
}

int response5(requester_t eve_req) {
    // Fulfill requests from eve
    fulfiller_t ful = socket_malloc_fulfiller(SOCK_TYPE_PUSH);

    // Connect
    socket_fulfiller_connect(ful, eve_req);

    // Reply
    sync_state_t ss;
    msg_delay_return(&ss);
    msg_resume_return(NULL, 0, 0, ss);

    // Create a certificate for our interface
    res_t reser = cap_malloc(RES_CERT_META_SIZE + sizeof(ful_pack));
    _safe cap_pair pair;
    cert_t ff_cert = rescap_take_authed(reser, &pair, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP, AUTH_CERT, own_auth, NULL, NULL).cert;

    assert(ff_cert);

    ful_pack* pack = (ful_pack*)pair.data;

    pack->data_arg = pack->oob_data_arg = UNTRUSTED_DATA;
    pack->ful = SEALED_CROSS_DOMAIN(ff);
    pack->ful_oob = NULL;
    pack->sub = NULL;

    socket_fulfill_progress_bytes_authorised(ful, SOCK_INF, F_CHECK | F_PROGRESS, ff_cert, NULL, 0);

    return 0;
}


int main(void) {

    namespace_register_found_id_authed(namespace_id_num_bob);
    namespace_register(namespace_num_bob, act_self_ref);

    // Make a certificate of the proper response
    res_t res = cap_malloc(RES_CERT_META_SIZE);
    response_cert = rescap_take_authed(res, NULL, CHERI_PERM_LOAD, AUTH_CERT, own_auth, NULL, READ_ONLY(PROPER_RESPONSE)).cert;

    // Make a certificate for the socket



    msg_enable = 1;

    return 0;
}


void (*msg_methods[]) = {&response1, &response2, &response3, &response4, response5};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);
