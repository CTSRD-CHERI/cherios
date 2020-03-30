/*-
 * Copyright (c) 2020 Lawrence Esswood
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
#ifndef CHERIOS_WCHAR_H
#define CHERIOS_WCHAR_H

#include "cdefs.h"
#include "time.h"
#include "stdio.h"
#include "stddef.h"
#include "locale.h"

// Placeholder
typedef long mbstate_t;

__BEGIN_DECLS

typedef int wint_t;

#define WEOF -1

// Not implemented

int fwprintf(FILE* __restrict stream, const wchar_t* __restrict format, ...);
int fwscanf(FILE* __restrict stream, const wchar_t* __restrict format, ...);
int swprintf(wchar_t* __restrict s, size_t n, const wchar_t* __restrict format, ...);
int swscanf(const wchar_t* __restrict s, const wchar_t* __restrict format, ...);
int vfwprintf(FILE* __restrict stream, const wchar_t* __restrict format, va_list arg);
int vfwscanf(FILE* __restrict stream, const wchar_t* __restrict format, va_list arg);  // C99
int vswprintf(wchar_t* __restrict s, size_t n, const wchar_t* __restrict format, va_list arg);
int vswscanf(const wchar_t* __restrict s, const wchar_t* __restrict format, va_list arg);  // C99
int vwprintf(const wchar_t* __restrict format, va_list arg);
int vwscanf(const wchar_t* __restrict format, va_list arg);  // C99
int wprintf(const wchar_t* __restrict format, ...);
int wscanf(const wchar_t* __restrict format, ...);
wint_t fgetwc(FILE* stream);
wchar_t* fgetws(wchar_t* __restrict s, int n, FILE* __restrict stream);
wint_t fputwc(wchar_t c, FILE* stream);
int fputws(const wchar_t* __restrict s, FILE* __restrict stream);
int fwide(FILE* stream, int mode);
wint_t getwc(FILE* stream);
wint_t getwchar();
wint_t putwc(wchar_t c, FILE* stream);
wint_t putwchar(wchar_t c);
wint_t ungetwc(wint_t c, FILE* stream);
double wcstod(const wchar_t* __restrict nptr, wchar_t** __restrict endptr);
float wcstof(const wchar_t* __restrict nptr, wchar_t** __restrict endptr);         // C99
long double wcstold(const wchar_t* __restrict nptr, wchar_t** __restrict endptr);  // C99
long wcstol(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);
long long wcstoll(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);  // C99
unsigned long wcstoul(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);
unsigned long long wcstoull(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);  // C99
wchar_t* wcscpy(wchar_t* __restrict s1, const wchar_t* __restrict s2);
wchar_t* wcsncpy(wchar_t* __restrict s1, const wchar_t* __restrict s2, size_t n);
wchar_t* wcscat(wchar_t* __restrict s1, const wchar_t* __restrict s2);
wchar_t* wcsncat(wchar_t* __restrict s1, const wchar_t* __restrict s2, size_t n);
int wcscmp(const wchar_t* s1, const wchar_t* s2);
int wcscoll(const wchar_t* s1, const wchar_t* s2);
int wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n);
size_t wcsxfrm(wchar_t* __restrict s1, const wchar_t* __restrict s2, size_t n);
wchar_t* wcschr(const wchar_t* s, wchar_t c);
size_t wcscspn(const wchar_t* s1, const wchar_t* s2);
size_t wcslen(const wchar_t* s);
wchar_t* wcspbrk(const wchar_t* s1, const wchar_t* s2);
wchar_t* wcsrchr(const wchar_t* s, wchar_t c);
size_t wcsspn(const wchar_t* s1, const wchar_t* s2);
wchar_t* wcsstr(const wchar_t* s1, const wchar_t* s2);
wchar_t* wcstok(wchar_t* __restrict s1, const wchar_t* __restrict s2, wchar_t** __restrict ptr);
wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n);
int wmemcmp(wchar_t* __restrict s1, const wchar_t* __restrict s2, size_t n);
wchar_t* wmemcpy(wchar_t* __restrict s1, const wchar_t* __restrict s2, size_t n);
wchar_t* wmemmove(wchar_t* s1, const wchar_t* s2, size_t n);
wchar_t* wmemset(wchar_t* s, wchar_t c, size_t n);
size_t wcsftime(wchar_t* __restrict s, size_t maxsize, const wchar_t* __restrict format,
                const tm* __restrict timeptr);
wint_t btowc(int c);
int wctob(wint_t c);
int mbsinit(const mbstate_t* ps);
size_t mbrlen(const char* __restrict s, size_t n, mbstate_t* __restrict ps);
size_t mbrtowc(wchar_t* __restrict pwc, const char* __restrict s, size_t n, mbstate_t* __restrict ps);
size_t wcrtomb(char* __restrict s, wchar_t wc, mbstate_t* __restrict ps);
size_t mbsrtowcs(wchar_t* __restrict dst, const char** __restrict src, size_t len,
                 mbstate_t* __restrict ps);
size_t wcsrtombs(char* __restrict dst, const wchar_t** __restrict src, size_t len,
                 mbstate_t* __restrict ps);

__END_DECLS


#endif //CHERIOS_WCHAR_H
