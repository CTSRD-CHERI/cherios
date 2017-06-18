/*-
 * Copyright (c) 2016 Hongyan Xia
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
#include "sys/mman.h"
#include "object.h"
#include "namespace.h"

static void * memmgt_ref = NULL;
static void * memmgt_id  = NULL;

#define MALLOC_FASTPATH

#ifndef MALLOC_FASTPATH
void *malloc(size_t length) {
	if(memmgt_ref == NULL) {
		memmgt_ref = namespace_get_ref(3);
		memmgt_id  = namespace_get_id(3);
	}
	return (void *)ccall_4(memmgt_ref, memmgt_id, 0,  length, 0, 0, 0);
}

void *calloc(size_t items, size_t length) {
	if(memmgt_ref == NULL) {
		memmgt_ref = namespace_get_ref(3);
		memmgt_id  = namespace_get_id(3);
	}
	return (void *)ccall_4(memmgt_ref, memmgt_id, 1,  items, length, 0, 0);
}

void *realloc(void *ptr, size_t length) {
	if(memmgt_ref == NULL) {
		memmgt_ref = namespace_get_ref(3);
		memmgt_id  = namespace_get_id(3);
	}
	return (void *)ccall_4(memmgt_ref, memmgt_id, 2,  (register_t)ptr, length, 0, 0);
}

void free(void *addr) {
	if(memmgt_ref == NULL) {
		memmgt_ref = namespace_get_ref(3);
		memmgt_id  = namespace_get_id(3);
	}
	ccall_4(memmgt_ref, memmgt_id, 3,  (register_t)addr, 0, 0, 0);
}

#else /* ifdef MALLOC_FASTPATH */
static int mallocInit = 0;
static void *memmgt_entry = NULL;
static void *memmgt_base = NULL;

void
init_memmgt() {
    memmgt_entry = namespace_get_entry(3);
    memmgt_base  = namespace_get_base(3);
    mallocInit = 1;
}

void *malloc(size_t length) {
    if(mallocInit == 0)
        init_memmgt();
	return (void *)dcall_4(0, length, 0, 0, 0, memmgt_entry, memmgt_base);
}

void *calloc(size_t items, size_t length) {
    if(mallocInit == 0)
        init_memmgt();
	return (void *)dcall_4(1, items, length, 0, 0, memmgt_entry, memmgt_base);
}

void *realloc(void *ptr, size_t length) {
    if(mallocInit == 0)
        init_memmgt();
	return (void *)dcall_4(2, (register_t)ptr, length, 0, 0, memmgt_entry, memmgt_base);
}

void free(void *ptr) {
    if(mallocInit == 0)
        init_memmgt();
	dcall_4(3, (register_t)ptr, 0, 0, 0, memmgt_entry, memmgt_base);
}
#endif /* MALLOC_FASTPATH */

void memmgt_set_act(void * ref, void * id) {
	memmgt_ref = ref;
	memmgt_id  = id;
}

void *calloc_core(size_t items, size_t length) {
	if(memmgt_ref == NULL) {
		memmgt_ref = namespace_get_ref(3);
		memmgt_id  = namespace_get_id(3);
	}
	return (void *)ccall_4(memmgt_ref, memmgt_id, 1,  items, length, 0, 0);
}

void free_core(void *addr) {
	if(memmgt_ref == NULL) {
		memmgt_ref = namespace_get_ref(3);
		memmgt_id  = namespace_get_id(3);
	}
	ccall_4(memmgt_ref, memmgt_id, 3,  (register_t)addr, 0, 0, 0);
}
