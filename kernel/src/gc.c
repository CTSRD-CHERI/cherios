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

#include "klib.h"

/* Experimental. Should not be in kernel.
 * Todo: Needs a good memory model to be made safe
 */

/* takes two tagged pointers */
static inline register_t derives_of(void * c, void * p) {
	size_t cb = cheri_getbase(c);
	size_t pb = cheri_getbase(p);
	size_t cl = cheri_getlen(c);
	size_t pl = cheri_getlen(p);
	size_t ce = cb+cl;
	size_t pe = pb+pl;
	#if 0
	if(!((pb<=cb) || (pb>=ce))) {
		CHERI_PRINT_CAP(c);
		CHERI_PRINT_CAP(p);
	}
	#endif
	kernel_assert((pb<=cb) || (pb>=ce)); /* cb pb   ce  */
	kernel_assert(!((pb<=cb) && (cb<=pe) && (pe<ce))); /* pb cb pe ce */
	if((cb>=pb) && (ce<=pe)) {
		return 1;
	}
	return 0;
}

int try_gc(void * p, void * pool) {
	#ifndef __LITE__
	kernel_printf(KBLD KCYN "GC! b:%16lx l:%16lx"KRST"\n",
	            cheri_getbase(p), cheri_getlen(p));
	#endif
	/*todo: (if userspace) we can be prempted here and memory could move */
	/*todo: also gc saved regs in kernel*/
	void **gc = pool;
	size_t len = cheri_getlen(gc) * sizeof(char) / sizeof(void *);
	for(size_t i=0; i<len; i++) {
		void * c = gc[i];
		if(!cheri_gettag(c)) {
			continue;
		}
		if(derives_of(c, p)) {
			gc[i] = cheri_cleartag(c);
			//return 0;
		}
	}
	return 1;
}
