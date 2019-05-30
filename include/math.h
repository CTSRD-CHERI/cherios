/*-
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2017 Lawrence Esswood
 * Copyright (c) 2016 SRI International
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

#ifndef _MATH_H_
#define _MATH_H_

#include "mips.h"

#ifndef __ASSEMBLY__

static inline int imax(int a, int b) {
	return (a>b ? a : b);
}

static inline int imin(int a, int b) {
	return (a<b ? a : b);
}

static inline size_t umax(size_t a, size_t b) {
	return (a>b ? a : b);
}

static inline size_t umin(size_t a, size_t b) {
	return (a<b ? a : b);
}

static inline int slog2(size_t s) {
	int i=-1;
	while(s) {
		i++;
		s >>= 1;
	}
	return i;
}

static inline int is_power_2(size_t x) {
	return (x & (x-1)) == 0;
}

static inline size_t align_up_to(size_t size, size_t align) {
	size_t mask = align - 1;
	return (size + mask) & ~mask;
}

static inline size_t align_down_to(size_t size, size_t align) {
	return size & ~(align-1);
}

static inline size_t round_up_to_nearest_power_2(size_t v) {
	v--;
	v |= v >> 1L;
	v |= v >> 2L;
	v |= v >> 4L;
	v |= v >> 8L;
	v |= v >> 16L;
	v |= v >> 32L;
	v++;
	return v;
}

#else // __ASEEMBLY__

#define ALIGN_UP_2(X, P)   		(((X) + ((1 << (P)) - 1)) &~ ((1 << (P)) - 1))
#define ALIGN_DOWN_2(X, P)  	((X) &~ ((1 << (P)) - 1))

#endif

#endif
