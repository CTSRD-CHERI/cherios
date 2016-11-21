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

#include "klib.h"

/*
 * Various util functions
 */

/* Converts RW pointer to RX pointer */
static inline void * kernel_cap_to_exec(const void * p) {
	/* XXXPM: won't this break if PCC has non-zero base?  Since
	   then, the setbounds will add base(PCC) to base(p) to get
	   the final base.

	   Also, what if p is derived from a default data cap,
	   resulting in no overlap of p with PCC?
	*/
	void * c = cheri_getpcc();
	c = cheri_setoffset(c, cheri_getbase(p));
	c = cheri_setbounds(c, cheri_getlen(p));
	c = cheri_setoffset(c, cheri_getoffset(p));
	return c;
}

static inline void * kernel_seal(const void * p, uint64_t otype) {
	void * seal = cheri_setoffset(cheri_getdefault(), otype);
	return cheri_seal(p, seal);
}

static inline void * kernel_unseal(void * p, uint64_t otype) {
	void * seal = cheri_setoffset(cheri_getdefault(), otype);
	return cheri_unseal(p, seal);
}
