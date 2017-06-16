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

#ifndef __LITE__

/*
 * Prints a nice regump
 */

#define REG_DUMP_M(_reg) {\
	register_t reg = kernel_exception_framep_ptr->mf_##_reg; \
	__REGDUMP(reg, reg, #_reg, 64); \
	}

#define REG_DUMP_C(_reg)
/*
	regdump_c(#_reg, creg++==reg_num, \
		kernel_exception_framep_ptr->cf_##_reg);
 */

#define __REGDUMP(elem, cond, name, bits) { \
	printf("%s"name":"KFNT"0x", cond?"":KFNT,elem); \
	int elem_lead_0 = bits - 3 - slog2(elem); \
	for(int i=0; i<elem_lead_0; i+=4) { printf("0");} \
	if(elem) { printf(KREG"%jx ", elem); } else { printf(" "KREG);} \
	}

/*
static void regdump_c(const char * str_cap, int hl, const void * cap) {
	printf("%s%-3s:"KREG, hl?KBLD KUND:"", str_cap);
	int tag  = cheri_gettag(cap);
	printf("%s", tag?" t:1 ":KFNT" t:0 "KREG);
	size_t base = cheri_getbase(cap);
	__REGDUMP(base, base||tag, "b", 64);
	size_t len = cheri_getlen(cap);
	__REGDUMP(len, len, "l", 64);
	size_t offset = cheri_getoffset(cap);
	__REGDUMP(offset, offset, "o", 64);
	size_t perm = cheri_getperm(cap);
	__REGDUMP(perm, perm||tag, "p", 32);
	int seal = cheri_getsealed(cap);
	printf("%s", seal?"s:1 ":KFNT"s:0 "KREG);
	size_t otype = cheri_gettype(cap);
	__REGDUMP(otype, otype||seal, "otype", 24);
	printf(KRST"\n");
}
 */

void regdump(int reg_num) {
	printf("Regdump:\n");

	REG_DUMP_M(at); REG_DUMP_M(v0); REG_DUMP_M(v1); printf("\n");

	REG_DUMP_M(a0); REG_DUMP_M(a1); REG_DUMP_M(a2); REG_DUMP_M(a3); printf("\n");

	REG_DUMP_M(t0); REG_DUMP_M(t1); printf("\n");

	REG_DUMP_M(s0); REG_DUMP_M(s1); REG_DUMP_M(s2); REG_DUMP_M(s3); printf("\n");

	REG_DUMP_M(t9); printf("\n");

	REG_DUMP_M(gp); REG_DUMP_M(sp); REG_DUMP_M(fp); REG_DUMP_M(ra); printf("\n");

	REG_DUMP_M(hi); REG_DUMP_M(lo); printf("\n");

	REG_DUMP_M(pc); printf("\n");

	printf("\n");
}

#else

void regdump(int reg_num __unused) {
	return;
}

#endif

void framedump(const struct reg_frame *frame) {
	struct reg_frame *tmp = kernel_exception_framep_ptr;
	kernel_exception_framep_ptr = (struct reg_frame *)frame;
	regdump(-1);
	kernel_exception_framep_ptr = tmp;
}
