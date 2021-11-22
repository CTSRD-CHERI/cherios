/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Hibler and Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)memset.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
/* #include <sys/cdefs.h> */
/* __FBSDID("$FreeBSD$"); */

#include "cheric.h"
#include "string.h"

#define	wsize	sizeof(u_int)
#define	wmask	(wsize - 1)

/*
 * XXXRW: This should in, in due course, be renamed to memset(), but currently
 * the compiler will rewrite memset() calls into memset_c() calls even for
 * pure-capability code.
 */
void *
memset_c(void *dst0, int c0, size_t length)
{
	size_t t;
	u_int c;
	u_char *dst;

	dst = dst0;
	/*
	 * If not enough words, just fill bytes.  A length >= 2 words
	 * guarantees that at least one of them is `complete' after
	 * any necessary alignment.  For instance:
	 *
	 *	|-----------|-----------|-----------|
	 *	|00|01|02|03|04|05|06|07|08|09|0A|00|
	 *	          ^---------------------^
	 *		 dst		 dst+length-1
	 *
	 * but we use a minimum of 3 here since the overhead of the code
	 * to do word writes is substantial.
	 */
	if (length < 3 * wsize) {
		while (length != 0) {
			*dst++ = c0;
			--length;
		}
		return (dst0);
	}

	if ((c = (u_char)c0) != 0) {	/* Fill the word. */
		c = (c << 8) | c;	/* u_int is 16 bits. */
#if UINT_MAX > 0xffff
		c = (c << 16) | c;	/* u_int is 32 bits. */
#endif
#if UINT_MAX > 0xffffffff
		c = (c << 32) | c;	/* u_int is 64 bits. */
#endif
	}
	/* Align destination by filling in bytes. */
	if ((t = (long)dst & wmask) != 0) {
		t = wsize - t;
		length -= t;
		do {
			*dst++ = c0;
		} while (--t != 0);
	}

	/* Fill words.  Length was >= 2*words so we know t >= 1 here. */
	t = length / wsize;
	do {
		*(u_int *)(void *)dst = c;
		dst += wsize;
	} while (--t != 0);

	/* Mop up trailing bytes, if any. */
	t = length & wmask;
	if (t != 0)
		do {
			*dst++ = c0;
		} while (--t != 0);
	return (dst0);
}

void *
memset(void *dst0, int c0, size_t length)
{

	return (memset_c(dst0, c0, length));
}

void
bzero(void *b, size_t s)
{

	(void)memset_c(b, 0, s);
}


#ifdef PLATFORM_riscv
// Until I write an appropriate memcpy for RISCV
void* memcpy(void* restrict s1, const void* restrict s2, size_t n) {
    if (((n % sizeof(void*)) == 0) &&
        ((size_t)s1 % sizeof(void*) == 0) &&
        ((size_t)s1 % sizeof(void*) == 0)) {
        void** dst = (void**)s1;
        void* const * src = (void* const *)s2;
        for (size_t i = 0; i != (n / sizeof(void*)); i++) {
            *(dst++) = *(src++);
        }
    } else {
        char* dst = (char*)s1;
        const char* src = (const char*)s2;
        for (size_t i = 0; i != n; i++) {
            *(dst++) = *(src++);
        }
    }

    return s1;
}
#endif
