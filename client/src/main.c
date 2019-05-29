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
#include "net.h"
#include "sockets.h"
#include "assert.h"
#include "stdio.h"
#include "unistd.h"

#define REQ1    "POST somefile.txt HTTP/1.0\n" \
                "Content-Length: 14\n" \
                "\n" \
                "Hello World!\n"
#define REQ2    "GET somefile.txt HTTP/1.0\n" \
                "\n" \

ssize_t TRUSTED_CROSS_DOMAIN(ful_print)(capability arg, char* buf, uint64_t offset, uint64_t length);
ssize_t ful_print(__unused capability arg, char* buf, uint64_t __unused offset, uint64_t length) {
    printf("%.*s",(int)length, buf);
    return length;
}

int main(__unused register_t arg, __unused capability carg) {

    // Connect to server
    struct tcp_bind bind;
    struct tcp_bind server;

    bind.port = 1234;
    bind.addr.addr = IP_ADDR_ANY->addr;

    inet_aton("127.0.0.1", &server.addr);
    server.port = 666;

    NET_SOCK netsock;

    while(net_try_get_ref() == NULL) {
        sleep(0);
    }
    // We need to loop. The server may not have been created yet!
    do {
        sleep(0);
        netsock_connect_tcp(&bind, &server, NULL);
        netsock = netsock_accept(MSG_NONE);
    } while(netsock == NULL);

    assert(netsock != NULL);

    printf("Sending request: %s\n", REQ1);
    ssize_t  res = socket_send(&netsock->sock, REQ1, sizeof(REQ1), MSG_NONE);

    assert_int_ex(res, ==, sizeof(REQ1));

    res = socket_fulfill_progress_bytes_unauthorised(netsock->sock.read.push_reader, SOCK_INF, F_CHECK | F_PROGRESS,
            TRUSTED_CROSS_DOMAIN(ful_print), NULL, 0, NULL, NULL, TRUSTED_DATA, NULL);

    close((FILE_t)netsock);

    bind.port = 1235;
    do {
        sleep(0);
        netsock_connect_tcp(&bind, &server, NULL);
        netsock = netsock_accept(MSG_NONE);
    } while(netsock == NULL);


    assert(netsock != NULL);

    printf("Sending request: %s\n", REQ2);
    res = socket_send(&netsock->sock, REQ2, sizeof(REQ2)-1, MSG_NONE);

    assert_int_ex(res, ==, sizeof(REQ2)-1);

    res = socket_fulfill_progress_bytes_unauthorised(netsock->sock.read.push_reader, SOCK_INF, F_CHECK | F_PROGRESS,
            TRUSTED_CROSS_DOMAIN(ful_print), NULL, 0, NULL, NULL, TRUSTED_DATA, NULL);

    assert_int_ex(res, > , 0);

    return 0;
}
