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

#ifndef __STDLIB_H__
#define	__STDLIB_H__

#include "cdefs.h"
#include "capmalloc.h"
#include "stdio.h"
#include "assert.h"
#include "stddef.h"
#include "locale.h"

__BEGIN_DECLS

static inline capability malloc(size_t size) {
    res_t res = cap_malloc(size);
    _safe cap_pair pair;
    pair.data = NULL;
    rescap_take(res, &pair);
    capability taken = pair.data;
    cap_free_handle(res);
    assert_int_ex(cheri_getlen(taken), >=, size);
    //taken = cheri_setbounds(taken, size); screws with free
    return taken;
}

static inline int posix_memalign(void **memptr, size_t alignment, size_t size) {
    size_t alignment_easy = (size <= RES_META_SIZE) ? size : RES_META_SIZE;
    assert(alignment <= alignment_easy);
    *memptr = malloc(size);
    return 0;
}

static inline capability malloc_arena_dma(size_t size, struct arena_t* arena, size_t* dma_off) {
    res_t res = cap_malloc_arena_dma(size, arena, dma_off);
    _safe cap_pair pair;
    rescap_take(res, &pair);
    capability taken = pair.data;
    return taken;
}

static inline capability malloc_debug(size_t size) {
    capability r = malloc(size);
    //printf("Allocated: "); CHERI_PRINT_CAP(r);
    static int x = 0;
    printf("Total malloc: %d", x++);
    return r;
}

static inline void free(capability cap) {
    cap_free(cap);
}

static inline void free_debug(capability cap) {
    //printf("Freed:      "); CHERI_PRINT_CAP(cap);
    static int x = 0;
    printf("Total free: %d", x++);
    cap_free(cap);
}

static inline void * calloc(size_t n, size_t s) {
    return malloc(n * s);
}

static inline void * calloc_debug(size_t n, size_t s) {
    return malloc_debug(n * s);
}

void 	abort(void)      __dead2;
void	exit(int status) __dead2;

char *  itoa ( int value, char * str, int base );
int     atoi(const char* str);

int     rand(void);

void  qsort(void	*base, size_t nmemb, size_t size,
int (*compar)(const void *, const void	*));

static char *getenv(__unused const char *name) {
    return NULL;
}


long strtol(const char *nptr, char **endptr, int base);

// Not implemented
double      atof(const char* str);
long        atol(const char *str);
long long   atoll(const char *str);
double      strtod(const char *__restrict str, char **__restrict str_end);
float       strtof(const char *__restrict str, char **__restrict str_end);
long double strtold(const char *__restrict str, char **__restrict str_end);
long long   strtoll(const char *__restrict str, char **__restrict str_end, int base);
unsigned long strtoul(const char *__restrict str, char **__restrict str_end, int base);
unsigned long long strtoull(const char *__restrict str, char **__restrict str_end, int base);

int mblen(const char* s, size_t n);
int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n);
int wctomb(char *s, wchar_t wc);
size_t mbstowcs(wchar_t *__restrict dst, const char *__restrict src, size_t len);
size_t wcstombs(char *__restrict dst, const wchar_t *__restrict src, size_t len);

void* bsearch(const void *key, const void *ptr, size_t count, size_t size, int (*comp)(const void*, const void*));

void srand(unsigned seed);
void *realloc(void *ptr, size_t new_size);

int system(const char *command);
int atexit(void (*func)(void));
int at_quick_exit(void (*func)(void));
void quick_exit(int exit_code) __dead2;
void _Exit(int exit_code) __dead2;

__END_DECLS

#endif /* !__STDLIB_H__ */
