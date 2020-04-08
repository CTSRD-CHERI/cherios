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

#include "sockets.h"
#include "cheric.h"
#include "thread.h"
#include "assert.h"
#include "stdio.h"
#include "string.h"
#include "capmalloc.h"

#define DATA_SIZE 0x800
#define INDIR_SIZE 16
#define PORT 777

#define BIG_TEST_SIZE (1 << 12)

const char* str1 = "Hello World!\n";
const int size1 = 14;
const char* str2 = "Some";
const int size2 = 4;
const char* str3 = " more bytes in parts\n";
const int size3 = 22;

const char* str4 = "Some more bytes in parts\n";

// For the poll test
int order[] = {0,0,1,1,0,1,2};

static void big_test_recv(unix_like_socket* sock) {
    // Test sending a large amount of data in small parts (all proxied from the first requester */

    ssize_t res;

    char buf[100];

    for(size_t i = 0; i != BIG_TEST_SIZE; i++) {
        res = socket_recv(sock, buf, 3, MSG_NONE);
        assert_int_ex(res, ==, 3);
        assert_int_ex(buf[0], ==, (char)(i & 0xFF));
        assert_int_ex(buf[1], ==, (char)((i>>8) & 0xFF));
        assert_int_ex(buf[2], ==, (char)((i>>16) & 0xFF));
    }
}

static void big_test_2_recv(unix_like_socket* sock) {
    // Test sending a large amount of data in large parts

    ssize_t res;

    char big_buf[DATA_SIZE/2];
    // Test sending a large number of bytes in large chunks

    for(size_t i = 0; i != 3; i++) {
        res = socket_recv(sock, big_buf, DATA_SIZE/2, MSG_NO_COPY_READ);
        assert_int_ex(res, ==, DATA_SIZE/2);
        for(size_t j = 0; j < DATA_SIZE/2; j++) {
            assert(big_buf[j] == (char)j);
        }
    }
}

static void big_test_send(unix_like_socket* sock) {
    ssize_t res;

    char buf[100];

    for(size_t i = 0; i != BIG_TEST_SIZE; i++) {
        buf[0] = (char)(i & 0xFF);
        buf[1] = (char)((i >> 8) & 0xFF);
        buf[2] = (char)((i >> 16) & 0xFF);
        res = socket_send(sock, buf, 3, MSG_NONE);
        assert(res == 3);
    }
}

static void big_test_send2(unix_like_socket* sock) {
    char big_buf[DATA_SIZE/2];
    ssize_t res;

    // Test sending a large number of bytes in large chunks
    for(size_t i = 0; i < DATA_SIZE/2; i++) {
        big_buf[i] = (char)i;
    }

    for(size_t i = 0; i != 3; i++) {
        res = socket_send(sock, big_buf, DATA_SIZE/2, MSG_NONE);
        assert(res == DATA_SIZE/2);
    }
}

// This tests sendfile
void con2_start(__unused register_t arg, __unused capability carg) {

    // The first send file

    unix_like_socket socket;

    unix_like_socket* sock = &socket;

    ssize_t res;

    sock->read.push_reader = socket_malloc_fulfiller(SOCK_TYPE_PUSH);
    assert(sock->read.push_reader);

    res = socket_init(sock, MSG_NONE, NULL, DATA_SIZE, CONNECT_PUSH_READ);
    assert_int_ex(res, ==, 0);

    res = socket_connect_via_rpc(carg, PORT+2, NULL, sock->read.push_reader);

    assert_int_ex(res, ==, 0);

    // This is for proxy
    big_test_recv(sock);

    // This is for join
    big_test_recv(sock);
    big_test_recv(sock);
    big_test_recv(sock);

    // Now close this socket

    res = socket_close(sock);
    assert_int_ex(res, ==, 0);

    // Now try with a requester

    assert_int_ex(res, ==, 0);

    unix_like_socket socket2;
    unix_like_socket* sock2 = &socket2;
    sock2->read.pull_reader = socket_new_requester(cap_malloc(SIZE_OF_request(INDIR_SIZE)), INDIR_SIZE, SOCK_TYPE_PULL, NULL).val;

    assert(sock2->read.pull_reader != NULL);

    res = socket_init(sock2, MSG_NO_COPY_WRITE, NULL, 0, CONNECT_PULL_READ);

    assert_int_ex(res, ==, 0);

    res = socket_connect_via_rpc(carg, PORT+3, sock2->read.pull_reader, NULL);

    assert_int_ex(res, ==, 0);

    big_test_2_recv(sock2);

    // Now close this socket

    res = socket_close(sock2);
    assert_int_ex(res, ==, 0);

    // Now try with a requester

    assert_int_ex(res, ==, 0);

    return;
}

ssize_t TRUSTED_CROSS_DOMAIN(con3_full)(capability arg, char* buf, uint64_t offset, uint64_t length);
__used ssize_t con3_full(__unused capability arg, __unused char* buf, __unused uint64_t offset, __unused uint64_t length) {
    assert(0);
    while(1);
}

ssize_t TRUSTED_CROSS_DOMAIN(con3_full2)(capability arg, char* buf, uint64_t offset, uint64_t length);
__used ssize_t con3_full2(capability arg, char* buf, uint64_t offset, uint64_t length) {
    char* data = (char*)arg;
    memcpy(buf, data+offset,length);
    return length;
}

ssize_t TRUSTED_CROSS_DOMAIN(con3_sub)(capability arg, uint64_t offset, uint64_t length, char** out_buf);
__used ssize_t con3_sub(capability arg, uint64_t offset, uint64_t length, char** out_buf) {
    // Gives the buffer out in small parts to test that multiple calls works
    char* data = (char*)arg;
    assert_int_ex(length+offset, ==, 3*BIG_TEST_SIZE);

    char* from = data + offset;

    size_t blob = (3*BIG_TEST_SIZE) / 64;
    size_t len = length > blob ? blob : length;

    *out_buf = from;

    return len;
}

typedef struct proxy_pair {
    fulfiller_t ff;
    requester_t req;
} proxy_pair;

void init_pair(proxy_pair* pp) {
    pp->ff = socket_malloc_fulfiller(SOCK_TYPE_PUSH);
    pp->req = socket_malloc_requester_32(SOCK_TYPE_PUSH, NULL);
    socket_fulfiller_connect(pp->ff, socket_make_ref_for_fulfill(pp->req));
    socket_requester_connect(pp->req);
}

char co3data[BIG_TEST_SIZE * 3];

void con3_start(__unused register_t arg, capability carg) {
    fulfiller_t ff = socket_malloc_fulfiller(SOCK_TYPE_PULL);
    assert(ff);

    __unused int result = socket_connect_via_rpc(carg, PORT + 5, NULL, ff);
    assert_int_ex(result, ==, 0);

    for(size_t i = 0; i != BIG_TEST_SIZE; i++) {
        co3data[(3 * i) + 0] = (char)(i & 0xFF);
        co3data[(3 * i) + 1] = (char)((i >> 8) & 0xFF);
        co3data[(3 * i) + 2] = (char)((i >> 16) & 0xFF);
    }

    // Send but offer buffers, error if normal fulfill is used
    ssize_t sent = socket_fulfill_progress_bytes_unauthorised(ff, BIG_TEST_SIZE*3, F_CHECK | F_PROGRESS,
                                           TRUSTED_CROSS_DOMAIN(con3_full), co3data, 0, NULL, TRUSTED_CROSS_DOMAIN(con3_sub),
                                           TRUSTED_DATA, NULL);

    assert_int_ex(sent, ==, BIG_TEST_SIZE * 3);

    // Send normally

    sent = socket_fulfill_progress_bytes_unauthorised(ff, BIG_TEST_SIZE*3, F_CHECK | F_PROGRESS,
                                                      TRUSTED_CROSS_DOMAIN(con3_full2), co3data, 0, NULL, NULL,
                                                      TRUSTED_DATA, NULL);

    assert_int_ex(sent, ==, BIG_TEST_SIZE * 3);

    // Send, offer a buffer, via a proxy
    sent = socket_fulfill_progress_bytes_unauthorised(ff, BIG_TEST_SIZE*3, F_CHECK | F_PROGRESS,
                                                      TRUSTED_CROSS_DOMAIN(con3_full), co3data, 0, NULL, TRUSTED_CROSS_DOMAIN(con3_sub),
                                                      TRUSTED_DATA, NULL);

    assert_int_ex(sent, ==, BIG_TEST_SIZE * 3);

    // TODO Wait everything to have completed as we want to allocate this buffer on the stack
    return;
}

void connector_start(__unused register_t arg, __unused capability carg) {
    capability data_buffer[DATA_SIZE/sizeof(capability)];

    unix_like_socket socket;

    unix_like_socket* sock = &socket;

    int res;

    sock->read.push_reader = socket_malloc_fulfiller(SOCK_TYPE_PUSH);
    assert(sock->read.push_reader);
    sock->write.pull_writer = socket_malloc_fulfiller(SOCK_TYPE_PULL);
    assert(sock->write.pull_writer);

    res = socket_init(sock, MSG_NONE, (char*)data_buffer, DATA_SIZE, CONNECT_PUSH_READ | CONNECT_PULL_WRITE);
    assert_int_ex(res, ==, 0);


    int result = socket_connect_via_rpc(carg, PORT, NULL, sock->read.push_reader);
    assert_int_ex(result, ==, 0);
    result = socket_connect_via_rpc(carg, PORT+1, NULL, sock->write.pull_writer);
    assert_int_ex(result, ==, 0);

    char buf[100];

    ssize_t rec = socket_recv(sock, buf, size1, MSG_NONE);
    assert_int_ex(rec, ==, size1);

    // Test a basic send
    assert(strcmp(buf, str1) == 0);

    // Test sending a message in parts

    ssize_t sent = socket_send(sock, str2, size2, MSG_NONE);

    assert(sent == size2);
    sent = socket_send(sock, str3, size3, MSG_NONE);

    assert(sent == size3);

    // Test sending a large amount of data in small parts
    big_test_recv(sock);

    big_test_2_recv(sock);

    // Test a proxy. Will need own requester

    thread_new("socket_part3", 0, act_self_ref, &con2_start);

    unix_like_socket socket2;
    unix_like_socket* sock2 = &socket2;
    sock2->write.push_writer = socket_new_requester(cap_malloc(SIZE_OF_request(INDIR_SIZE)), INDIR_SIZE, SOCK_TYPE_PUSH, NULL).val;

    assert_int_ex(res, ==, 0);

    res = socket_init(sock2, MSG_NO_COPY_WRITE, NULL, 0, CONNECT_PUSH_WRITE);
    assert_int_ex(res, ==, 0);

    res = socket_listen_rpc(PORT+2, sock2->write.push_writer, NULL);
    assert_int_ex(res, == , 0);

    sent = socket_sendfile(sock2, sock, 3 * BIG_TEST_SIZE);
    assert_int_ex(sent, ==, 3 * BIG_TEST_SIZE);

    // Test join. Send to the same file but from our own pull requester

    thread_new("socket_part4", 0, act_self_ref, &con3_start);

    unix_like_socket socket4;
    unix_like_socket* sock4 = &socket4;
    sock4->read.pull_reader = socket_new_requester(cap_malloc(SIZE_OF_request(INDIR_SIZE)), INDIR_SIZE, SOCK_TYPE_PULL, NULL).val;

    assert_int_ex(res, ==, 0);

    res = socket_init(sock4, MSG_NO_COPY_WRITE, NULL, 0, CONNECT_PULL_READ);
    assert_int_ex(res, ==, 0);

    res = socket_listen_rpc(PORT+5, sock4->read.pull_reader, NULL);
    assert_int_ex(res, == , 0);

    // Send twice, without drb
    sent = socket_sendfile(sock2, sock4, 3 * BIG_TEST_SIZE);
    assert_int_ex(sent, ==, 3 * BIG_TEST_SIZE);

    // Now attach a drb. Must align to a capability.

    capability drb_buf[BIG_TEST_SIZE/(8 * sizeof(capability))];
    init_data_buffer(&sock2->write_copy_buffer,(char*)drb_buf,BIG_TEST_SIZE/8);
    socket_requester_set_drb(sock2->write.push_writer, &sock2->write_copy_buffer);

    // And sendfile agagain
    sent = socket_sendfile(sock2, sock4, 3 * BIG_TEST_SIZE);
    assert_int_ex(sent, ==, 3 * BIG_TEST_SIZE);

    socket_requester_wait_all_finish(sock4->read.pull_reader, 0);

    // and again, but this time use join_proxy
    proxy_pair pp;
    init_pair(&pp);

    socket_requester_space_wait(sock4->read.pull_reader, 1, 0, 0);
    socket_requester_space_wait(sock2->write.push_writer, 1, 0, 0);

    rec = socket_request_proxy_join(sock4->read.pull_reader, pp.req,
                                       NULL, 3 * BIG_TEST_SIZE, 0,
                                       sock2->write.push_writer, pp.ff, 0);

    assert_int_ex(rec, ==, 0);

    rec = socket_close(sock4);
    assert_int_ex(rec, ==, 0);

    rec = socket_close(sock2);
    assert_int_ex(rec, ==, 0);

    // Test a fulfill -> fulfill send file

    unix_like_socket socket3;
    unix_like_socket* sock3 = &socket3;

    sock3->write.pull_writer = socket_malloc_fulfiller(SOCK_TYPE_PULL);
    assert(sock3->write.pull_writer);

    res = socket_init(sock3, MSG_NONE, NULL, 0, CONNECT_PULL_WRITE);
    assert_int_ex(res, ==, 0);

    res = socket_listen_rpc(PORT+3, NULL, sock3->write.pull_writer);
    assert_int_ex(res, == , 0);

    sent = socket_sendfile(sock3, sock, 3 * BIG_TEST_SIZE);
    assert_int_ex(sent, ==, 3 * (DATA_SIZE/2));

    rec = socket_close(sock3);
    assert_int_ex(rec, ==, 0);

    // Test copying capabilities

    capability cap_rec;

    rec = socket_recv(sock, (char*)&cap_rec, sizeof(capability), MSG_NONE);
    assert_int_ex(rec, ==, sizeof(capability));
    assert(cheri_gettag(cap_rec));
    rec = socket_recv(sock, buf, 1, MSG_NONE);
    assert(rec == 1);
    rec = socket_recv(sock, (char*)&cap_rec, sizeof(capability), MSG_NONE);
    assert(rec = sizeof(capability));
    assert(cheri_gettag(cap_rec));
    rec = socket_recv(sock, (char*)&cap_rec, sizeof(capability), MSG_NONE);
    assert(rec = sizeof(capability));
    assert(!cheri_gettag(cap_rec));
    rec = socket_recv(sock, (char*)&cap_rec, sizeof(capability), MSG_NO_CAPS);
    assert(rec = sizeof(capability));
    assert(!cheri_gettag(cap_rec));

    // Test poll. Re-use 3
    sock3->read.push_reader = sock3->write.pull_writer;
    sock3->write.pull_writer = NULL;
    res = socket_reuse_fulfiller(sock3->read.push_reader, SOCK_TYPE_PUSH);

    assert_int_ex(res, ==, 0);

    res = socket_init(sock3, MSG_NONE, NULL, 0, CONNECT_PUSH_READ);
    assert_int_ex(res, ==, 0);

    res = socket_connect_via_rpc(carg, PORT+4, NULL, sock3->read.push_reader);
    assert_int_ex(res, ==, 0);

    poll_sock_t socks[2];
    socks[0].fd = sock;
    socks[0].events = POLL_IN;
    socks[1].fd = sock3;
    socks[1].events = POLL_IN;

    for(int i = 0; i != 7; i++) {
        int use_sock = order[i];

        if(use_sock == 2) {
            enum poll_events events;
            __unused int poll_r = socket_poll(NULL, 0, -1, &events);
            assert_int_ex(poll_r, ==, 1);
            assert_int_ex(events, ==, POLL_IN);
            next_msg();
            continue;
        }

        // unix_like_socket* ssock = use_sock == 0 ? sock : sock2;

        __unused int poll_r = socket_poll(socks, 2, -1, 0);

        assert_int_ex(poll_r, ==, 1);
        assert_int_ex(socks[use_sock].revents, ==, POLL_IN);
        assert_int_ex(socks[1-use_sock].revents, ==, POLL_NONE);

        rec = socket_recv(socks[use_sock].fd, buf, size1, MSG_NONE);
        assert_int_ex(rec, ==, size1);

        // Test a basic send
        assert(strcmp(buf, str1) == 0);
    }


    // Test the closing mechanic

    rec = socket_close(sock);
    assert_int_ex(rec, ==, 0);
}

int main(__unused register_t arg, __unused capability carg) {

    printf("Socket test Hello World!\n");

    thread t = thread_new("socket_part2", 0, act_self_ref, &connector_start);

    capability data_buffer[DATA_SIZE/CAP_SIZE];

    unix_like_socket socket;

    unix_like_socket* sock = &socket;

    ERROR_T(requester_t) new_req =
            socket_new_requester(cap_malloc(SIZE_OF_request(INDIR_SIZE)), INDIR_SIZE, SOCK_TYPE_PUSH, &sock->write_copy_buffer);

    assert(IS_VALID(new_req));

    sock->write.push_writer = new_req.val;

    new_req = socket_new_requester(cap_malloc(SIZE_OF_request(INDIR_SIZE)), INDIR_SIZE, SOCK_TYPE_PULL, NULL);

    assert(IS_VALID(new_req));

    sock->read.pull_reader = new_req.val;

    assert(sock->write.push_writer != NULL);
    assert(sock->read.pull_reader != NULL);

    int res = socket_init(sock, MSG_NONE, (char*)data_buffer, DATA_SIZE, CONNECT_PUSH_WRITE | CONNECT_PULL_READ);
    assert_int_ex(res, ==, 0);

    res = socket_listen_rpc(PORT, sock->write.push_writer,NULL);
    assert_int_ex(res, ==, 0);
    res = socket_listen_rpc(PORT+1, sock->read.pull_reader,NULL);
    assert_int_ex(res, ==, 0);

    ssize_t sent = socket_send(sock, str1, size1, MSG_NONE);
    assert(sent == size1);

    char buf[100];

    size_t p1 = 13;
    size_t p2 = size2 + size3 - p1;

    ssize_t rec = socket_recv(sock, buf, p1, MSG_NO_COPY_READ);
    assert_int_ex(rec, ==, p1);
    rec = socket_recv(sock, buf + p1, p2, MSG_NO_COPY_READ);
    assert_int_ex(rec, ==, p2);

    // Test multiple sends with partial reads
    assert(strcmp(buf, str4) == 0);

    // Test sending a large number of bytes in small bits and bit bits
    big_test_send(sock);
    big_test_send2(sock);

    // For proxying

    big_test_send(sock);
    big_test_send2(sock);

    // Test copying capabilities

    capability cap = act_self_ref;
    sent = socket_send(sock, (char*)&cap, sizeof(capability), MSG_NONE);
    assert(sent == sizeof(capability));
    sent = socket_send(sock, buf, 1, MSG_NONE);
    assert(sent == 1);
    sent = socket_send(sock, (char*)&cap, sizeof(capability), MSG_NONE);
    assert(sent == sizeof(capability));
    sent = socket_send(sock, (char*)&cap, sizeof(capability), MSG_NO_CAPS);
    assert(sent == sizeof(capability));
    sent = socket_send(sock, (char*)&cap, sizeof(capability), MSG_NONE);
    assert(sent == sizeof(capability));

    // Test polling


    unix_like_socket socket2;

    unix_like_socket* sock2 = &socket2;

    sock2->write.push_writer = socket_new_requester(cap_malloc(SIZE_OF_request(INDIR_SIZE)), INDIR_SIZE, SOCK_TYPE_PUSH, &sock2->write_copy_buffer).val;

    assert_int_ex(res, ==, 0);

    capability data_buffer2[DATA_SIZE/CAP_SIZE];

    socket_init(sock2, MSG_NONE, (char*)data_buffer2, DATA_SIZE, CONNECT_PUSH_WRITE);
    assert_int_ex(res, ==, 0);

    res = socket_listen_rpc(PORT+4, sock2->write.push_writer,NULL);
    assert_int_ex(res, ==, 0);

    act_kt t_act = syscall_act_ctrl_get_ref(get_control_for_thread(t));

    for(int i = 0; i != 7; i++) {
        int use_sock = order[i];

        if(use_sock == 2) {
            message_send(0,0,0,0,NULL,NULL,NULL,NULL, t_act, SEND, 0);
            continue;
        }
        unix_like_socket* ssock = use_sock == 0 ? sock : sock2;

        // No copy will result in block until finishing so we can get good testing of select where one socket is blocked and the other is not
        sent = socket_send(ssock, str1, size1, MSG_NO_COPY_WRITE);
        assert_int_ex(sent, ==, size1);
    }

    // Test the closing mechanic

    rec = socket_recv(sock, buf, 1, MSG_NO_COPY_READ);
    assert_int_ex(rec, ==, E_SOCKET_CLOSED);
    rec = socket_send(sock, buf, 1, MSG_NONE);
    assert_int_ex(rec, ==, E_SOCKET_CLOSED);

    rec = socket_close(sock);

    // We sent some requests expecting an error - they may be oustanding
    if(rec == E_SOCKET_CLOSED) rec = 0;

    assert_int_ex(rec, ==, 0);

    printf("Socket test finished\n");

    return 0;
}
