/*-
 * Copyright (c) 2016 Lawrence Esswood
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

#include "elf.h"
#include "mips.h"
#include "misc.h"
#include "stdlib.h"
#include "sys/mman.h"
#include "utils.h"

#define TMP_ALLOC_ALIGN 0x1000

static char * pool_end = NULL;
static char * pool_next = NULL;

static char * pool_ex = NULL;

cap_pair tmp_alloc(size_t s, Elf_Env* unused __unused) {
	s = roundup2(s, TMP_ALLOC_ALIGN);
	if(pool_next + s >= pool_end) {
		return (cap_pair){.code = NULL, .data = NULL};
	}
	void * p = pool_next;
	p = cheri_setbounds_exact(p, s);
	pool_next += s;

	return (cap_pair){.code = rederive_perms(p, pool_ex), .data = p};
}

void init_tmp_alloc(cap_pair pool) {
	char * pool_start = pool.data;

    size_t align_up = (size_t)(-((size_t)pool_start)) & (TMP_ALLOC_ALIGN-1);

	size_t pool_remaining = cheri_getlen(pool_start) - cheri_getoffset(pool_start) - align_up;
	pool_start = cheri_setbounds(pool_start + align_up, pool_remaining);

	pool_end = pool_start + pool_remaining;

	pool_ex = pool.code;
	pool_next = pool_start;
}

cap_pair get_remaining(void) {
	size_t pool_remaining = cheri_getlen(pool_next) - cheri_getoffset(pool_next);
	char * pool_start = cheri_setbounds(pool_next, pool_remaining);

	cap_pair ret = (cap_pair){.code = rederive_perms(pool_start, pool_ex), .data = pool_start};

	pool_next = pool_end;

	return ret;
}

void tmp_free(void * p __unused, Elf_Env unused __unused) {
	/* init alloc has no free */
}
