/*-
 * Copyright (c) 1983 Regents of the University of California.
 * Copyright (c) 2015 SRI International
 * Copyright (c) 2016 Robert N. M. Watson
 * All rights reserved.
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
/*static char *sccsid = "from: @(#)malloc.c	5.11 (Berkeley) 2/23/91";*/
static char *rcsid = "$FreeBSD$";
#endif /* LIBC_SCCS and not lint */

/*
 * malloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this
 * implementation, the available sizes are 2^n-4 (or 2^n-10) bytes long.
 * This is designed for use in a virtual memory environment.
 */

#include "mips.h"
#include "kernel.h"
#include "cheric.h"
#include "klib.h"
#include "lib.h"

#include "malloc_heap.h"

#pragma clang diagnostic ignored "-Wsign-compare"

caddr_t		 pagepool_start, pagepool_end;
static void	*pool;

int
__morepages(int n __unused)
{

	kernel_panic("%s", __func__);
}

void
__init_heap(size_t pagesz)
{
	size_t heaplen;
	void *heap;

	/*
	 * XXXBD: assumes DDC is page aligned.
	 */
	kernel_assert((size_t)&__start_heap == roundup2((size_t)&__start_heap,
	    pagesz));

	heaplen = (size_t)&__stop_heap - (size_t)&__start_heap;
	heap = cheri_setoffset(cheri_getdefault(), (size_t)&__start_heap);
	heap = cheri_csetbounds(heap, heaplen);
	kernel_assert(cheri_getoffset(heap) == 0);
	kernel_assert(cheri_getlen(heap) == heaplen);

	pagepool_start = (size_t)heap;
	pagepool_end = pagepool_start + heaplen;
	pool = heap;
}

void *
__rederive_pointer(void *ptr)
{
	caddr_t addr, base;

	addr = cheri_getbase(ptr) + cheri_getoffset(ptr);
	base = cheri_getbase(pool);

	if (addr >= base && addr < base + cheri_getlen(pool))
		return(cheri_setoffset(pool, addr - base));

	return (NULL);
}
