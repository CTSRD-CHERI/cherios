/*-
 * Copyright (c) 2016 Robert N. M. Watson
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

#ifndef __STRING_H__
#define	__STRING_H__

#include "cdefs.h"
#include "stddef.h"
#include "locale.h"

__BEGIN_DECLS
void	bzero(void *, size_t);
void *	memcpy(void *dest, const void *src, size_t n);
void * memmove ( void * destination, const void * source, size_t num );
void *	memset(void *, int, size_t);
char *	strchr(const char * s, int c);
char *  strrchr(const char *cp, int ch);
char *	strcpy(char * dest,const char *src);
char *  strcat ( char * destination, const char * source );
char * strncat(char *dst, const char *src, size_t n);
size_t strcspn(const char * __restrict s, const char * __restrict charset);
size_t strspn(const char *s, const char *charset);
char * strdup(const char *str1);
void *  memchr( const void * ptr, int value, size_t num );
int	strcmp(const char *s1, const char *s2);
size_t	strlen(const char *str);
int	strncmp(const char * cs,const char * ct,size_t count);
char *	strncpy(char * dest,const char *src,size_t count);
char *  strstr(const char *s, const char *find);
char *strpbrk(const char *str1, const char *str2);

int memcmp(const void *ptr1, const void *ptr2, size_t num);

#define strcoll strcmp
#define strxfrm strncpy

static char *strerror(__unused int errnum) {
    // TODO: There are actually strings defined via macro in errno.h to use
    return NULL;
}

__END_DECLS

#endif /* !__STRING_H__ */
