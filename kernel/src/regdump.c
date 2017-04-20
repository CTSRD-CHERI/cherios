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

#include <activations.h>
#include "klib.h"
#include "stdio.h"

//#ifndef __LITE__
#ifdef notyet

/*
 * Prints a nice regump
 */

#define REG_DUMP_M(_reg) {\
	register_t reg = kernel_exception_framep_ptr->mf_##_reg; \
	__REGDUMP(reg, reg, #_reg, 64); \
	}

#define REG_DUMP_C(_reg) \
	regdump_c(#_reg, creg++==reg_num, \
		kernel_exception_framep_ptr->cf_##_reg);

#define __REGDUMP(elem, cond, name, bits) { \
	printf("%s"name":"KFNT"0x", cond?"":KFNT,elem); \
	int elem_lead_0 = bits - 3 - slog2(elem); \
	for(int i=0; i<elem_lead_0; i+=4) { printf("0");} \
	if(elem) { printf(KREG"%jx ", elem); } else { printf(" "KREG);} \
	}

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

static inline size_t correct_base(size_t image_base, capability pcc) {
	return ((cheri_getoffset(pcc) + cheri_getbase(pcc)) - image_base);
}

static inline void print_frame(int num, size_t ra, char * sp) {
	printf("%2d| [0x%016lx] (sp=%p)\n", num, ra, sp);
}

static inline void backtrace(size_t image_base, char* stack_pointer, capability return_address) {
	int i = 0;

	// Function prolog:
	// daddiu  sp,sp,-size			// allocates space
	// ...
	// csc     c17,sp,offset(c11)		// stores return address

	// Instruction Format

	// daddiu 	rs,rt, im				// |011001|rs   |rt   |im(16)
	// |011001|11101|11101|im(16)

	// csc     cs,rt,im(cb)				// |111110|cs   |cb   |rt   |im(11)
	// |111110|10001|01011|11101|im(11)

	uint32_t daddiu_form_mask = 0xFFFF0000;
	uint32_t csc_form_mask    = 0xFFFFF800;
	uint32_t daddiu_form_val  = 0b0110011110111101U << 16U;
	uint32_t csc_form_val	  = 0b111110100010101111101U << 11U;
	uint32_t daddiu_i_mask 	  = (1 << 16) - 1;
	uint32_t csc_i_mask		  = (1 << 11) - 1;

	// FIXME assumes a function prolog with daddiu. Not true for leaf functions

	do {
		print_frame(i++, correct_base(image_base, return_address), stack_pointer);

		//scan backwards for daddiu
		int16_t stack_size = 0;
		int16_t offset = 0;
		for(uint32_t* instr = ((uint32_t*)return_address);; instr--) {
			if(cheri_getoffset(instr) > cheri_getlen(instr)) {
				printf("***bad frame***\n");
				return;
			}
			uint32_t val = *instr;
			if((val & daddiu_form_mask) == daddiu_form_val) {
				stack_size = (int16_t)(val & daddiu_i_mask);
				break;
			}
			if((val & csc_form_mask) == csc_form_val) {
				offset = (int16_t)((val & csc_i_mask) << 4);
			}
		}

		capability * ra_ptr = ((capability *)((stack_pointer + offset)));

		if(cheri_getoffset(ra_ptr) > cheri_getlen(ra_ptr)) {
			printf("***bad frame***\n");
			return;
		}

		return_address = *ra_ptr;
		// Offset by 2 instructions for the cjal + nop
		return_address = (capability)((uint32_t*)return_address-2);
		stack_pointer = stack_pointer - stack_size;
	} while(cheri_getoffset(stack_pointer) != cheri_getlen(stack_pointer));
	print_frame(i++, correct_base(image_base, return_address), stack_pointer);
}

void regdump(int reg_num) {
	int creg = 0;
	printf("Regdump:\n");
	kernel_assert(kernel_exception_framep_ptr == &(kernel_curr_act->saved_registers));

	REG_DUMP_M(at); REG_DUMP_M(v0); REG_DUMP_M(v1); printf("\n");

	REG_DUMP_M(a0); REG_DUMP_M(a1); REG_DUMP_M(a2); REG_DUMP_M(a3); printf("\n");
	REG_DUMP_M(a4); REG_DUMP_M(a5); REG_DUMP_M(a6); REG_DUMP_M(a7); printf("\n");

	REG_DUMP_M(t0); REG_DUMP_M(t1); REG_DUMP_M(t2); REG_DUMP_M(t3); printf("\n");

	REG_DUMP_M(s0); REG_DUMP_M(s1); REG_DUMP_M(s2); REG_DUMP_M(s3); printf("\n");
	REG_DUMP_M(s4); REG_DUMP_M(s5); REG_DUMP_M(s6); REG_DUMP_M(s7); printf("\n");

	REG_DUMP_M(t8); REG_DUMP_M(t9); printf("\n");

	REG_DUMP_M(gp); REG_DUMP_M(sp); REG_DUMP_M(fp); REG_DUMP_M(ra); printf("\n");

	REG_DUMP_M(hi); REG_DUMP_M(lo); printf("\n");

	#if 0
	REG_DUMP_M(pc); printf("\n"); /* does not seem in sync with pcc */
	#endif

	printf("\n");

	REG_DUMP_C(c0); printf("\n");

	REG_DUMP_C(c1); REG_DUMP_C(c2); printf("\n");

	REG_DUMP_C(c3); REG_DUMP_C(c4); REG_DUMP_C(c5); REG_DUMP_C(c6);  printf("\n");
	REG_DUMP_C(c7); REG_DUMP_C(c8); REG_DUMP_C(c9); REG_DUMP_C(c10); printf("\n");

	REG_DUMP_C(c11); REG_DUMP_C(c12); REG_DUMP_C(c13);
	REG_DUMP_C(c14); REG_DUMP_C(c15); printf("\n");

	REG_DUMP_C(c16); REG_DUMP_C(c17); printf("\n");

	REG_DUMP_C(c18); REG_DUMP_C(c19); REG_DUMP_C(c20); REG_DUMP_C(c21); printf("\n");
	REG_DUMP_C(c22); REG_DUMP_C(c23); REG_DUMP_C(c24); REG_DUMP_C(c25); printf("\n");

	REG_DUMP_C(idc); creg = 31; REG_DUMP_C(pcc); printf("\n");

	size_t offset_index = correct_base(kernel_curr_act->image_base, kernel_exception_framep_ptr->cf_pcc);
	printf("pcc at %lx in %s\n", offset_index, kernel_curr_act->name);

	printf("\nLoaded images:\n");
	for(size_t i = 0; i < kernel_next_act; i++) {
		act_t* act = &kernel_acts[i];
		printf("%16s: %lx\n", act->name, act->image_base);
	}

	printf("\nAttempting backtrace:\n\n");
	char * stack_pointer = (char*)kernel_curr_act->saved_registers.cf_c11 + kernel_curr_act->saved_registers.mf_sp;
	capability return_address = kernel_curr_act->saved_registers.cf_pcc;
	backtrace(kernel_curr_act->image_base, stack_pointer, return_address);
}

#else

void regdump(int reg_num __unused) {
	return;
}

#endif

