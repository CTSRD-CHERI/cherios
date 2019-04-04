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

char LOREM[BIG_SIZE] = "\n"
"\n"
"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Ut ut dignissim sapien. In eu nunc et dolor efficitur ultrices. Duis sit amet metus at quam sodales fringilla. Donec consequat felis felis, at iaculis ante malesuada id. Pellentesque metus sapien, cursus ac libero in, dictum fermentum quam. Phasellus scelerisque, purus vel condimentum maximus, arcu sem placerat odio, id lobortis ante orci at purus. Nullam mattis ultricies iaculis. Etiam maximus nunc vitae tempor commodo. Donec vel est velit. Proin eget justo ante.\n"
"\n"
"Nam id ornare massa. Nunc tincidunt maximus gravida. Cras nibh nulla, fermentum in enim vitae, luctus ultrices odio. Integer vitae velit ipsum. Vivamus nisi nibh, pulvinar sit amet neque a, blandit pharetra risus. Maecenas nisi felis, sollicitudin consectetur faucibus dignissim, finibus a lacus. Curabitur tempor odio nec aliquam efficitur. Mauris volutpat, turpis a dapibus condimentum, tortor dui tempus dui, ac vulputate lectus turpis nec sem. Donec ac sollicitudin ex. Mauris non augue vitae tortor vulputate egestas. Etiam sed urna nec metus interdum imperdiet. Nulla volutpat massa nec lectus accumsan volutpat. Donec posuere posuere ligula ut fermentum.\n"
"\n"
"Nulla pretium ante tortor, nec ultricies est tristique vitae. Nullam metus tellus, malesuada sit amet malesuada rutrum, maximus vel justo. Ut ac rhoncus enim. Maecenas et scelerisque enim. Phasellus quis commodo enim. Integer lobortis enim vitae nisi scelerisque posuere. Integer at laoreet ex.\n"
"\n"
"Integer sollicitudin pulvinar diam, eget feugiat arcu congue at. Pellentesque vitae feugiat augue. Vestibulum ut turpis a metus condimentum ullamcorper sit amet non dui. Donec rhoncus hendrerit gravida. Curabitur iaculis velit vitae tempor tempus. Sed mollis ultricies neque non ullamcorper. Mauris vel neque malesuada, porttitor erat vitae, rutrum arcu. Nunc id ante sapien. Vivamus facilisis justo ipsum, ac consequat libero ornare elementum. Cras nec odio eget ligula ornare molestie non ac ex. Aliquam ullamcorper sed velit eu ornare. Interdum et malesuada fames ac ante ipsum primis in faucibus. Cras pharetra luctus egestas. Ut quis rhoncus turpis, et imperdiet ligula.\n"
"\n"
"Nulla ornare rutrum diam non bibendum. Proin non dapibus orci, et euismod nulla. Praesent bibendum, dolor sed ullamcorper interdum, ipsum lectus ultrices justo, a rutrum sem eros vitae ipsum. Nunc vel sollicitudin mi, a accumsan nisl. Vestibulum in tempus arcu. Fusce condimentum venenatis gravida. Suspendisse lacinia massa in suscipit ornare. Aliquam erat volutpat. Vivamus sed tempor sem.\n"
"\n"
"Morbi luctus risus sit amet nibh accumsan elementum. Ut ac quam placerat, pulvinar libero sollicitudin, vehicula tellus. Aenean sed felis vel velit feugiat ullamcorper. Praesent finibus tempor facilisis. Maecenas tincidunt vel mauris et tristique. Donec sit amet ex vel ligula imperdiet condimentum. Fusce maximus risus sit amet purus blandit, vitae mollis sem pulvinar. Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. In hac habitasse platea dictumst. In euismod viverra congue. Quisque quis varius lectus. Sed congue diam id sapien eleifend ultrices. Duis iaculis sodales porta. Donec pharetra lobortis tellus a egestas.\n"
"\n"
"Morbi eros nisl, ullamcorper quis sem non, iaculis efficitur neque. Duis consectetur justo ut sapien viverra, ut facilisis felis pulvinar. Quisque tempor neque justo, eu aliquet massa maximus ut. Praesent sodales vulputate luctus. Sed quis molestie lacus. Maecenas metus mi, aliquet at sem et, ultricies dignissim dolor. Donec pharetra nec diam eu vulputate. Curabitur facilisis auctor luctus. Sed scelerisque mi eu erat facilisis, porttitor iaculis erat tempor. Nullam semper fringilla diam eget venenatis. Curabitur sagittis magna ac eros feugiat maximus. Sed ultrices massa a magna convallis, sit amet aliquet ante molestie. Donec finibus viverra tellus vitae egestas.\n"
"\n"
"Curabitur interdum, nunc a ullamcorper aliquet, nibh metus faucibus nulla, ut elementum nisl ante id justo. Vestibulum lacus elit, laoreet sit amet posuere et, rhoncus eu turpis. Curabitur fringaa";

char dest[BIG_SIZE];

int main(register_t arg, capability carg) {

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

    result = strcmp(buf, message);

    assert_int_ex(result, == , 0);

    result = close(file);

    assert_int_ex(result, ==, 0);

    char chars[] = {'0','1','2'};

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
    FILE_t file3 = open_encrypted("enc1", FA_OPEN_ALWAYS | FA_WRITE, MSG_NO_COPY, key, iv);
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
}
