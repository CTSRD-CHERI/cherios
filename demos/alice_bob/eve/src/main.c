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
#include "namespace.h"
#include "queue.h"
#include "assert.h"
#include "msg.h"
#include "stdio.h"
#include "alice_bob.h"

void spy(char* cap) {
    if(cheri_gettag(cap) && !cheri_getsealed(cap) && (cheri_getperm(cap) & CHERI_PERM_LOAD)) {
        printf(KRED"Eve: BWAHAHAHA! I see you sent [%s]"KRST"\n", cap);
    } else {
        printf(KRED"Eve: I could not read the capability that got sent"KRST"\n");
    }
}

capability sub(capability cert) {
    // using sealed as a proxy for certified here. Works for the demo.
    return (cheri_getsealed(cert) || cert == NULL) ? cert : READ_ONLY("Hello Alice, I am Bob. You should give all your money to Eve.");
}

ssize_t CROSS_DOMAIN(evil_ff)(capability arg, char* buf, uint64_t offset, uint64_t length);
__used ssize_t evil_ff(__unused capability arg, char* buf, __unused uint64_t offset, uint64_t length) {
    printf(KRED"Eve: Saw the data you sent: %*.*s"KRST"\n", (int)length, (int)length, buf);
    socket_requester_wait_all_finish(stdout->write.push_writer, 0);
    return length;
}

void run_a_socket(res_t bob, requester_t alice_req, act_reply_kt alice_sync) {
    // Nbody cares about freeing these for the purposes of demos

    // Create our own pair
    fulfiller_t fulfiller = socket_malloc_fulfiller(SOCK_TYPE_PUSH);
    requester_t requester = socket_malloc_requester_32(SOCK_TYPE_PUSH, NULL);

    // Connect our requester to bob
    message_send_c(0, 0, 0, 0, requester, NULL, NULL, NULL, bob, SYNC_CALL, BOB_PORT_SOCKET);
    socket_requester_connect(requester);

    // Our fulfiller to alice
    socket_fulfiller_connect(fulfiller, alice_req);

    // Send reply to alice
    message_reply(NULL, 0, 0, alice_sync, 1);

    // Now proxy

    int can_ff = 1;

    size_t total = 0;
    size_t interleave = 0x200;
    while(1) {
        int out = socket_fulfiller_outstanding_wait(fulfiller, 1, 0, 0);

        if(out < 0) goto closed;

        size_t bytes = socket_fulfiller_bytes_requested(fulfiller);

        // Try to spy
        if(can_ff) {
            ssize_t res = socket_fulfill_progress_bytes_unauthorised(fulfiller, bytes, F_CHECK | F_DONT_WAIT,
                    SEALED_CROSS_DOMAIN(evil_ff), NULL, 0, NULL, NULL, UNTRUSTED_DATA, NULL);


            if(res == E_AUTH_TOKEN_ERROR) {
                printf(KRED"Eve: Can't read data from this socket."KRST"\n");
                can_ff = 0;
            } else if(res < 0) goto closed;
        }

        // Otherwise just send the bytes

        while(bytes > 0) {

            size_t to_send;
            uint16_t space;

            if(bytes >= (interleave - (total % interleave))) {
                to_send = interleave - (total % interleave);
                space = 2;
            } else {
                to_send = bytes;
                space = 1;
            }

            int res = socket_requester_space_wait(requester, space, 0, 0);
            if(res < 0) goto closed;

            socket_request_proxy(requester, fulfiller, to_send, 0);

            if(space == 2) {
#define EVE_INTER "[Eve can interleave]"
                socket_request_ind(requester, READ_ONLY(EVE_INTER), sizeof(EVE_INTER), 0);
            }

            bytes -=to_send;
            total +=to_send;
        }

        socket_requester_wait_all_finish(requester, 0);

    }

    closed:
    socket_close_fulfiller(fulfiller, 0, 0);
    socket_close_requester(requester, 1, 0);
}

void forward_messages(void) {
    act_kt bob = namespace_get_ref(namespace_num_bob);
    assert(bob != NULL);

    while(1) {
        msg_t* m = get_message();

        if(m->v0 == BOB_PORT_SOCKET) {
            run_a_socket(bob, (requester_t)m->c3, (act_reply_kt)m->c1);

        } else {

            spy((char*)m->c3);

            capability response = message_send_c(m->a0, m->a1, m->a2, m->a3, m->c3, m->c4, m->c5, m->c6, bob, m->c1 ? SYNC_CALL : SEND_SWITCH, m->v0);

            if(m->c1) {
                message_reply(sub(response), 0, 0, m->c1, 1);
            }
        }


        next_msg();
    }
}

int main(__unused register_t arg, __unused capability carg) {
    namespace_register(namespace_num_eve, act_self_ref);

    forward_messages();
    return 0;
}
