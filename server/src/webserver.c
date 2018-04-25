/*-
 * Copyright (c) 2018 Lawrence Esswood
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
#include "webserver.h"
#include "stdio.h"
#include "unistd.h"
#include "colors.h"
#include "stdlib.h"
#include "namespace.h"
#include "assert.h"
#include "netsocket.h"

struct session {
    char file_name_buf[100];
    size_t server_root_length;
    unix_like_socket* sock;
    int sent_initial;
    int sent_headers;
};

#define ERR(...)    printf(__VA_ARGS__)
#define ER_R(...)  do { ERR(__VA_ARGS__); return -1;} while(0)

#define ROOT "/websrv"

#define LEN_HDR "Content-Length"

#define HTTP_VER "HTTP/1.0 "
#define NOT_FOUND "File not found"
#define YOU_SUCK "You suck"
#define WE_SUCK "This server sucks"

int send_response_initial(struct session* s, int code, const char* reason, size_t reason_len) {

    ssize_t ret = socket_send(s->sock, HTTP_VER, sizeof(HTTP_VER)-1, MSG_NONE);

    if(ret != sizeof(HTTP_VER)-1) return -1;

    char str_code[] = "000 ";
    itoa(code, str_code, 10);
    str_code[3] = ' ';

    ret = socket_send(s->sock, str_code, 4, MSG_NONE);

    if(ret != 4) return -1;

    ret = socket_send(s->sock, reason, reason_len, MSG_NONE);

    if(ret != reason_len) return -1;

    s->sent_initial = 1;

    return 0;
}

int send_ok(struct session* s) {
    return send_response_initial(s, 200, "OK" "\n", 3);
}

int send_header(struct session*s, char* header, size_t hdr_len, char* value, size_t value_len) {
    ssize_t ret = socket_send(s->sock, header, hdr_len, MSG_NONE);
    if(ret != hdr_len) return -1;
    ret = socket_send(s->sock, value, value_len, MSG_NONE);
    if(ret != value_len) return -1;
    ret = socket_send(s->sock, "\n", 1, MSG_NONE);
    if(ret != 1) return -1;
    return 0;
}

int send_content_length(struct session*s, int length) {
    char len_buf[100];
    itoa(length, len_buf, 10);
    size_t l = strlen(len_buf);
    return send_header(s, LEN_HDR ": ", sizeof(LEN_HDR)+1, len_buf, l);
}

int finish_headers(struct session*s) {
    ssize_t ret = socket_send(s->sock, "\n", 1, MSG_NONE);
    s->sent_headers = 1;
    if(ret == 1) return 0;
    else return -1;
}

int handle_get_post(struct session* s, struct initial* ini) {
    struct header hdr;


    ssize_t result;

    size_t file_size = 0;

    do {
        result = parse_header(&s->sock->read.push_reader, &hdr);
        if(result < 0)
            ER_R("Error parsing headers\n");
        if(result == 0) {
            if(strcmp(LEN_HDR, hdr.header) == 0) {
                int res = atoi(hdr.value);
                if(res < 0) ER_R("Negative sized length\n");
                file_size = (size_t)res;
            } else {
                printf("Ignoring header %s\n", hdr.header);
            }
        }

    } while(result == 0);

    if(ini->method == POST && file_size == 0) ER_R("POST should include a file size\n");

    if(ini->method == GET && file_size == 0) ER_R("My shitty FS has no way go get file size currently so plz say\n");

    FILE_t f = open(ini->file, 1, 1, MSG_NONE);

    if(f == NULL) {
        send_response_initial(s, 404, NOT_FOUND "\n", sizeof(NOT_FOUND));
        ER_R("Error opening file %s\n", ini->file);
    }


    if(ini->method == GET) {
        send_ok(s);
        send_content_length(s, (int)file_size);
        finish_headers(s);
        result = sendfile((FILE_t)s->sock, f, file_size);
    } else {
        result = sendfile(f,(FILE_t)s->sock, file_size);
    }

    close(f);

    if(result < 0 || result != file_size){
        if(ini->method == POST) {
            send_response_initial(s, 500, WE_SUCK "\n", sizeof(WE_SUCK));
            finish_headers(s);
        }
        ER_R("Error in sendfile %d\n", (int)-result);
    }

    if(ini->method == POST) {
        send_ok(s);
    }

    return 0;
}


int handle_request(struct session* s) {
    struct initial ini;
    ini.file = s->file_name_buf;

    int result = parse_initial(&s->sock->read.push_reader, &ini,
                               ini.file + s->server_root_length, sizeof(s->file_name_buf) - s->server_root_length);

    if(result < 0) ER_R("Error parsing initial line\n");

    switch (ini.method) {
        case GET:
        case POST:
            return handle_get_post(s, &ini);
        default:
            ER_R("Error: Unsupported mode %d\n", ini.method);
    }
}



void handle_loop(void) {
    struct session s;
    memcpy(s.file_name_buf, ROOT "/", sizeof(ROOT));
    s.server_root_length = sizeof(ROOT);

    ssize_t res = mkdir(ROOT);

    assert(res == 0 || res == 8);

    assert(namespace_register(namespace_num_webserver, act_self_ref) == 0);



    while(1) {
        netsocket* netsock = (netsocket*)malloc(sizeof(netsocket));
        s.sock = &netsock->sock;
        netsocket_init(netsock);

        res = netsocket_listen(netsock, 8080);

        if(res < 0) {
            printf("Rejected a connection\n");
            continue;
        }

        if(res == 0) {

            s.sent_initial = 0;
            s.sent_headers = 0;
            handle_request(&s);

            if(!s.sent_initial) {
                send_response_initial(&s, 400, YOU_SUCK "\n", sizeof(YOU_SUCK));
            }
            if(!s.sent_headers) {
                finish_headers(&s);
            }

        } else ERR("Listen failed %ld", -res);

        res = socket_close(s.sock);

        assert_int_ex(res, ==, 0);

    }
}
