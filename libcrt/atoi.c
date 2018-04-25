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
#include "math.h"
#include "stdio.h"

#define SUPPORT_BASE_MIN 2
#define SUPPORT_BASE_MAX 16

#define SUPPORT_BASES (SUPPORT_BASE_MAX+1-SUPPORT_BASE_MIN)

char base_chars[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
uint8_t log_2[] = {0,0,1,0,2,0,0,0,3,0,0,0,0,0,0,0,4};

size_t itoa_p2( int value, char* str, int base) {
    size_t ptr = 0;
    size_t shift = log_2[base];
    size_t mask = base-1;
    size_t n = 0;
    do {
        size_t digit = value & mask;
        value = (value >> shift);
        str[ptr++] = base_chars[digit];
        n++;
    } while(value != 0);

    str[ptr] = '\0';
    return n;
}

size_t itoa_div( int value, char * str, int base ) {
    size_t ptr = 0;
    size_t n = 0;
    do {
        size_t digit = value % base;
        value = (value / base);
        str[ptr++] = base_chars[digit];
        n++;
    } while(value != 0);

    str[ptr] = '\0';
    n;
}

char*  itoa ( int value, char * str, int base ) {
    if(base < SUPPORT_BASE_MIN || base > SUPPORT_BASE_MAX) return NULL;

    size_t n;
    if(is_power_2(base)) {
        n = itoa_p2(value, str, base);
    } else {
        n = itoa_div(value, str, base);
    }

    for(size_t i = 0; i != n / 2; i++) {
        char c1 = str[i];
        str[i] = str[n-1-i];
        str[n-1-i] = c1;
    }

    return str;
}

int atoi(const char* str) {

    unsigned const char* s = (unsigned const char*)str;

    int result = 0;
    int sign = 1;
    unsigned char c;

    while((c = *(s++)) == ' ' || c == '\t');

    if(c == '-' || c == '+') {
        if (c == '-') {
            sign = -1;
        }
        c = *(s++);
    }

    c = (c - (unsigned char)'0');
    while(c < 10) {
        result = (result*10) + c;
        c = (*(s++) - (unsigned char)'0');
    }

    return (result * sign);
}