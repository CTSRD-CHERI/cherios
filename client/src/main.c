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
#include "namespace.h"
#include "syscalls.h"
#include "netsocket.h"
#include "sockets.h"
#include "assert.h"
#include "stdio.h"

#define REQ1    "POST somefile.txt HTTP/1.0\n" \
                "Content-Length: 14\n" \
                "\n" \
                "Hello World!\n"
#define REQ2    "GET somefile.txt HTTP/1.0\n" \
                "\n" \

static ssize_t ful_print(capability arg, char* buf, uint64_t offset, uint64_t length) {
    printf("%.*s",(int)length, buf);
    return length;
}

int main(register_t arg, capability carg) {

    // Get the server

    act_kt server;

    while((server = namespace_get_ref(namespace_num_webserver)) == NULL) {
        sleep(0);
    }

    CHERI_PRINT_CAP(server);

    netsocket netsock;

    netsocket_init(&netsock);

    ssize_t res = netsocket_connect(&netsock, 8080, server);
    assert_int_ex(netsock.write_req.r.connected, ==, 1);
    assert_int_ex(res, ==, 0);

    printf("Sending request: %s\n", REQ1);
    res = socket_send(&netsock.sock, REQ1, sizeof(REQ1), MSG_NONE);

    assert_int_ex(res, ==, sizeof(REQ1));

    res = socket_internal_fulfill_progress_bytes(&netsock.sock.read.push_reader, SOCK_INF, 1, 1, 0, 0, ful_print, NULL, 0, NULL);

    socket_close(&netsock.sock);

    netsocket_init(&netsock);

    res = netsocket_connect(&netsock, 8080, server);
    assert_int_ex(netsock.write_req.r.connected, ==, 1);
    assert_int_ex(res, ==, 0);

    printf("Sending request: %s\n", REQ2);
    res = socket_send(&netsock.sock, REQ2, sizeof(REQ2), MSG_NONE);

    assert_int_ex(res, ==, sizeof(REQ2));

    res = socket_internal_fulfill_progress_bytes(&netsock.sock.read.push_reader, SOCK_INF, 1, 1, 0, 0, ful_print, NULL, 0, NULL);

    assert_int_ex(res, > , 0);
}
