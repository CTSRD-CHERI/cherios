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
#include "unistd.h"
#include "assert.h"
#include "stdio.h"
#include "string.h"
#include "aes.h"

#define BIG_SIZE 0x1000

#include "lorem.h"

char dest[BIG_SIZE];

int main(__unused register_t arg, __unused capability carg) {

    while(try_get_fs() == NULL) {
        sleep(0);
    }

    printf("Fs test start..\n");

    FILE_t file = open("foobar", FA_OPEN_ALWAYS | FA_WRITE | FA_READ, MSG_NONE);

    assert(file != NULL);

    char buf[100];

    const char message[] = "Hello World! Yay a message!\n";
    size_t ms = sizeof(message);

    ssize_t result;

    result = write(file, message, ms);

    assert_int_ex(result, ==, ms);

    result = lseek(file, 0, SEEK_SET);

    assert_int_ex(result, ==, 0);

    result = read(file, buf, ms);

    assert_int_ex(result, ==, ms);

    result = strncmp(buf, message, ms);

    assert_int_ex(result, == , 0);

    result = close(file);

    assert_int_ex(result, ==, 0);

    file = open("bigtest", FA_OPEN_ALWAYS | FA_WRITE | FA_READ, MSG_NONE);

    assert(file != NULL);

    result = write(file, LOREM, BIG_SIZE);

    assert_int_ex(-result, ==, -BIG_SIZE);

    result = lseek(file, 0, SEEK_SET);
    assert_int_ex(result, ==, 0);

    result = read(file, dest, BIG_SIZE);
    assert_int_ex(result, ==, BIG_SIZE);

    for(size_t i = 0; i < BIG_SIZE; i++) {
        assert_int_ex(LOREM[i], ==, dest[i]);
        dest[i] = 0;
    }

    FILE_t file2 = open("Target", FA_OPEN_ALWAYS | FA_WRITE | FA_READ, MSG_NONE);

    result = lseek(file, 0, SEEK_SET);
    assert_int_ex(result, ==, 0);

    result = sendfile(file2, file, BIG_SIZE);
    assert_int_ex(result, ==, BIG_SIZE);

    flush(file);

    result = close(file);
    assert_int_ex(result, ==, 0);

    flush(file2);

    result = lseek(file2, 0, SEEK_SET);
    assert_int_ex(result, ==, 0);

    result = read(file2, dest, BIG_SIZE);
    assert_int_ex(result, ==, BIG_SIZE);

    result = socket_requester_space_wait(file2->read.pull_reader, SPACE_AMOUNT_ALL, 1, 0);

    assert_int_ex(result, ==, 0);

    int once = 1;
    for(size_t i = 0; i < BIG_SIZE; i++) {
        if(LOREM[i] != dest[i]) {
            printf("Problem with %lx\n", i);
            if(once--) {
                for(size_t j = 0; j < BIG_SIZE; j++) {
                    if(j % 16 == 0) printf("\n");
                    if(j % (16 * 32) == 0) printf("\n");
                    printf("(%d|%d) ", LOREM[j], dest[j]);
                }
            }
        }
        assert_int_ex(LOREM[i], ==, dest[i]);
    }

    result = close(file2);
    assert_int_ex(result, ==, 0);

    printf("Normal Fs test success! Trying Encryption\n");


    uint8_t key[AES_KEYLEN];
    uint8_t iv[AES_BLOCKLEN];

    for(uint8_t i = 0; i != AES_KEYLEN; i++) key[i] = i;
    for(uint8_t i = 0; i != AES_BLOCKLEN; i++) iv[i] = i;

    // Write an encrypted file
    FILE_t file3 = open_encrypted("enc1", FA_OPEN_ALWAYS | FA_WRITE, MSG_NO_COPY_WRITE, key, iv);
    assert(file3 != NULL);

    result = write(file3, LOREM, BIG_SIZE);
    assert_int_ex(result, ==, BIG_SIZE);

    result = close(file3);
    assert_int_ex(result, ==, 0);

    FILE_t file4 = open("enc1", FA_OPEN_ALWAYS | FA_READ, MSG_NONE);
    assert(file4 != NULL);

    bzero(dest, BIG_SIZE);
    result = read(file4, dest, BIG_SIZE);

    assert_int_ex(result, ==, BIG_SIZE);
    result = close(file4);
    assert_int_ex(result, == ,0);


    //printf("This should be encrypted: %.*s\n", BIG_SIZE, dest);

    result = 1;
    for(size_t i = 0; i != BIG_SIZE; i++) {
        if(dest[i] != LOREM[i]) {
            result = 0;
            break;
        }
    }


    assert(result == 0);

    FILE_t file5 = open_encrypted("enc1", FA_OPEN_ALWAYS | FA_READ, MSG_NONE, key, iv);

    assert(file5 != NULL);

    bzero(dest, BIG_SIZE);
    result = read(file5, dest, BIG_SIZE);
    assert_int_ex(result, ==, BIG_SIZE);
    result = close(file5);
    assert_int_ex(result, == ,0);

    result = 1;
    for(size_t i = 0; i != BIG_SIZE; i++) {
        if(dest[i] != LOREM[i]) {
            result = 0;
            break;
        }
    }

    assert(result == 1);

    //printf("This should be decrypted: %.*s\n", BIG_SIZE, dest);

    printf("Fs test success!\n");

    return 0;
}
