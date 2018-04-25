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

#include "cheric.h"
#include "webserver.h"
#include "string.h"
#include "sockets.h"
#include "stdio.h"

struct copy_until_status {
    char* to;
    char delim;
    int done;
};

static ssize_t ful_expect(capability arg, char* buf, uint64_t offset, uint64_t length) {
    int res = memcmp((char*)arg+offset,buf,length);
    return (res == 0) ? (ssize_t)length : E_USER_FULFILL_ERROR;
}

static ssize_t ful_copy_until(capability arg, char* buf, uint64_t offset, uint64_t length) {
    struct copy_until_status *copy = (struct copy_until_status*)arg;
    if(copy->done) return 0;
    for(uint64_t i = 0; i < length; i++) {
        char c = buf[i];
        if(c == copy->delim) {
            copy->to[offset + i] = '\0';
            copy->done = 1;
            return i+1;
        }
        switch(c) {
            case '\n':
            case '\0':
                copy->done = -1;
                return i;
            default:
                copy->to[offset + i] = c;
        }
    }
    return length;
}

#define POP(A) \
    do { \
    res = socket_internal_fulfill_progress_bytes(push_read,1,1,1,0,0,copy_out, (capability)&A, 0, NULL);\
    if(res != 1) return -1; \
    } while(0)

#define EXPECT(X) \
    do { \
        res = socket_internal_fulfill_progress_bytes(push_read, sizeof(X)-1, 1, 1, 0, 0, ful_expect, (capability)X, 0, NULL);\
        if(res != sizeof(X) -1) return -1;\
    } while(0)

int parse_initial(uni_dir_socket_fulfiller* push_read, struct initial* initial, char* name_to, size_t name_to_length) {
    char a;
    ssize_t res;

    enum http_method method;

    POP(a);
    switch (a) {
        case 'C':
            EXPECT("ONNECT ");
            method = GET;
            break;
        case 'G':
            EXPECT("ET ");
            method = GET;
            break;
        case 'H':
            EXPECT("EAD ");
            method = HEAD;
            break;
        case 'O':
            EXPECT("PTIONS ");
            method = OPTIONS;
            break;
        case 'T':
            EXPECT("RACE ");
            method = TRACE;
            break;
        case 'P':
            POP(a);
            switch (a) {
                case 'A':
                    EXPECT("TCH ");
                    method = PATCH;
                    break;
                case 'O':
                    EXPECT("ST ");
                    method = POST;
                    break;
                case 'U':
                    EXPECT("T ");
                    method = PUT;
                    break;
                default:
                    return -1;
            }
            break;

        default:
            return -1;
    }

    initial->method = method;

    struct copy_until_status status;
    status.to = name_to;
    status.done = 0;
    status.delim = ' ';
    res = socket_internal_fulfill_progress_bytes(push_read, name_to_length, 1, 1, 0, 0, &ful_copy_until, (capability)&status, 0, NULL);

    if(res < 0 || status.done != 1) {
        return -1;
    }

    EXPECT("HTTP/1.0\n");

    return 0;
}


ssize_t parse_header(uni_dir_socket_fulfiller* push_read, struct header* header) {
    char a;
    ssize_t res;

    POP(a);

    if(a == '\n') return 1;

    struct copy_until_status status;
    header->header[0] = a;
    status.to = header->header+1;
    status.done = 0;
    status.delim = ':';

    res = socket_internal_fulfill_progress_bytes(push_read, sizeof(header->header)-1, 1, 1, 0, 0, &ful_copy_until, (capability)&status, 0, NULL);

    if(res < 0 || status.done != 1) return  -1;

    status.to = header->value;
    status.done = 0;
    status.delim = '\n';

    res = socket_internal_fulfill_progress_bytes(push_read, sizeof(header->value), 1, 1, 0, 0, &ful_copy_until, (capability)&status, 0, NULL);

    if(res < 0 || status.done != 1) return -1;

    return 0;
}