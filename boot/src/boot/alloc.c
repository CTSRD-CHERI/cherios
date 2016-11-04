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
#include "string.h"
#include "stdlib.h"
#include "sys/mman.h"
#include "object.h"

static inline void *align_upwards(void *p, uintptr_t align)
{
    align = 1 << align;
    uint8_t * addr = (uint8_t *)p;
    uintptr_t offset = (uintptr_t)addr - ((uintptr_t)addr & ~(align-1));
    if(offset > 0) {
    addr += align - offset;
    }
    return (void *)addr;
}

static const size_t pool_size = 1024*1024;
static char pool[pool_size];

static char * pool_start = NULL;
static char * pool_end = NULL;
static char * pool_next = NULL;

static int system_alloc = 0;

static void *boot_alloc_core(size_t s) {
	if(pool_next + s >= pool_end) {
		return NULL;
	}
	void * p = pool_next;
	p = __builtin_memcap_bounds_set(p, s);
	pool_next = align_upwards(pool_next+s, 4096);
	return p;
}

void boot_alloc_init(void) {
	pool_start = (char *)(pool);
	pool_end = pool + pool_size;
	pool_start = __builtin_memcap_bounds_set(pool_start, pool_size);
	pool_start = __builtin_memcap_perms_and(pool_start, 0b11111101); /* Remove eXe perm */
	pool_next = pool_start;
	bzero(pool, pool_size);
	system_alloc = 0;
}

void boot_alloc_enable_system(void * c_memmgt) {
	mmap_set_act(act_ctrl_get_ref(c_memmgt), act_ctrl_get_id(c_memmgt));
	system_alloc = 1;
}

void *boot_alloc(size_t s) {
	if(system_alloc == 1) {
		void * p = mmap(NULL, s, PROT_RW, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		if(p == MAP_FAILED) {
			return NULL;
		}
		return p;
	}
	return boot_alloc_core(s);
}

void boot_free(void * p __unused) {
	if(system_alloc == 1) {
		/* fixme: use munmap */
	}
	/* Boot alloc has no free */
}
