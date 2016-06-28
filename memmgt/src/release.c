/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "lib.h"

typedef  struct
{
	int	used;
	void *	p;
	int	r;
}  rel_t;

static const int release_pending_size = 0x10;
static rel_t release_pending[release_pending_size];
static int iteration = 0;

static inline size_t safe(size_t i) {
	return i%release_pending_size;
}

static void rel_push(void * p) {
	static size_t lastfree = 0;
	size_t i = lastfree;
	while(lastfree - i <  release_pending_size) {
		i++;
		size_t j = safe(i);
		if(release_pending[j].used == 0) {
			lastfree = j;
			release_pending[j].used = 1;
			release_pending[j].p = p;
			release_pending[j].r = iteration;
			return;
		}
	}
	panic("cannot store pointer to release");
}

static int try_gc(void * p) {
	register_t ret;
	__asm__ __volatile__ (
		"li    $v0, 66       \n"
		"cmove $c3, %[p]     \n"
		"cmove $c4, %[pool]  \n"
		"syscall             \n"
		"move %[ret], $v0    \n"
		: [ret] "=r" (ret)
		: [p] "C" (p), [pool] "C" (pool)
		: "v0", "$c3", "$c4");
	return ret;
}

static void try_gc_rel(void) {
	for(size_t i=0; i<release_pending_size; i++) {
		if(release_pending[i].used > 0 &&
		   release_pending[i].r >= iteration) {
			void * p = release_pending[i].p;
			if(try_gc(p)) {
				mfree(p);
				release_pending[i].used = 0;
			} else {
				release_pending[i].r =
				  iteration + release_pending[i].used;
				release_pending[i].used <<= 1;
			}
		}
	}
}

void release(void * p) {
	rel_push(p);
	try_gc_rel();
	iteration++;
}

void release_init(void) {
	for(size_t i=0; i<release_pending_size; i++) {
		release_pending[i].used = 0;
	}
}

