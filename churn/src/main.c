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
#include "mman.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"

#define BLOCK_SIZE  0x10000
#define WINDOW_SIZE 100
#define N           0x20000    // set to 0 for good times

int main(register_t arg, capability carg) {

    printf("Churn Hello World!\n");
    cap_pair pr;

    capability maps[WINDOW_SIZE];

    bzero(maps, sizeof(maps));

    printf("Oh boy here I go mmapping again!\n");

    for(size_t i = 0; i < N; i++) {
        size_t ndx = (i % WINDOW_SIZE);

        if(i + WINDOW_SIZE < N) {
            if( mmap_new(0, BLOCK_SIZE, CHERI_PERM_ALL, MAP_ANONYMOUS | MAP_PRIVATE, &pr) != 0) {
                printf("mmap failed\n");
                return -1;
            }
        }

        capability  old = maps[ndx];
        maps[ndx] = pr.data;

        if(old != NULL) {
            if(munmap(old, 0) != 0) {
                printf("munmap failed\n");
                return -1;
            }
        }
    }

    printf("Churn test done \n");

    return 0;
}
