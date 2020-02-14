/*-
 * Copyright (c) 2016 Hadrien Barral
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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

#include "ctype.h"
#include "mips.h"

int toupper(int c) {
	if(islower(c)) {
		c += 'A'-'a';
	}
	return c;
}

int tolower(int c) {
	if(isupper(c)) {
		c += 'a'-'A';
	}
	return c;
}

char * strtoupper(char * s) {
	char* p = s;
	while (*p != '\0') {
		*p = (char)toupper(*p);
		p++;
	}

	return s;
}

int isupper(int c) {
    return c>='A' && c <= 'Z';
}

int islower(int c) {
    return c>='a' && c<='z';
}

int isascii(int c) {
    return (c >> 7) == 0;
}

int isalpha(int c) {
    return isupper(c) || islower(c);
}

int isdigit(int c) {
	return ((char)c >= '0') && ((char)c <= '9');
}

int isspace(int c) {
    uint64_t set =
            (1ULL << ' ') | (1ULL << '\n') | (1ULL << '\t') | (1ULL << '\v') | (1ULL << '\f') |(1ULL << '\r');

    return (set & (1 << c)) != 0;
}