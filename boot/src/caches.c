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

#include "mips.h"
#ifdef CHERIOS_BOOT

#include "boot/boot.h"
#define assert(e) boot_assert(e)

#else  /* CHERIOS_BOOT */

#include "assert.h"

#endif /* CHERIOS_BOOT */

/* Having this here is dangerous for reasons
 *
static int line_size = 0;

static void cache_init(void) {
	register_t config1;
	__asm__ __volatile__ ("dmfc0 %0, $16, 1" : "=r" (config1));
	register_t il = (config1 >> 19) & 0b111;
	register_t dl = (config1 >> 10) & 0b111;
	assert(il == dl);
	assert((il>0) && (il<7));
	line_size = 1 << (il + 1);
}


static void cache_inv_low(size_t line) {
	__asm __volatile__(
		"cache %[op], 0(%[line]) \n"
		:: [op]"i" ((0b100 << 2) + 0), [line]"r" (line));
	__asm __volatile__(
		"cache %[op], 0(%[line]) \n"
		:: [op]"i" ((0b100 << 2) + 1), [line]"r" (line));
}

static void cache_invalidate(size_t addr, size_t size) {
	size_t line_mask = ~(line_size-1);
	size_t end  = addr + size + line_size;
	size_t line = addr & line_mask;
	while (line < end) {
		cache_inv_low(line);
		line += line_size;
	}
	__asm volatile("sync");
}

void caches_invalidate(void * addr, size_t size) {
	if(!line_size) {
		cache_init();
	}
	cache_invalidate((size_t)addr, size);
	cache_invalidate((size_t)addr, size);
}

*/