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
#include "activations.h"
#include "klib.h"
#include "cp0.h"

#ifndef __LITE__

/*
 * Prints a nice regump
 */

#define printf kernel_printf

#define REG_DUMP_M(_reg, _extra) {\
	register_t reg = frame->mf_##_reg; \
	__REGDUMP(reg, reg, #_reg _extra, 64); \
	}

#define REG_DUMP_C(_reg) \
	regdump_c(#_reg, creg++==reg_num, \
		frame->cf_##_reg, "");

#define REG_DUMP_C_extra(_reg, extra) \
	regdump_c(#_reg, creg++==reg_num, \
		frame->cf_##_reg, extra);

#define __REGDUMP(elem, cond, name, bits) { \
	printf("%s"name":"KFNT"0x", cond?"":KFNT); \
	int elem_lead_0 = bits - 3 - slog2(elem); \
	for(int i=0; i<elem_lead_0; i+=4) { printf("0");} \
	if(elem) { printf(KREG"%jx ", elem); } else { printf(" "KREG);} \
	}

static void regdump_c(const char * str_cap, int hl, const void * cap, const char* extra) {
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
	printf("%s"KRST"\n", extra);
}

static inline size_t correct_base(size_t image_base, capability pcc) {
	return ((cheri_getoffset(pcc) + cheri_getbase(pcc)) - image_base);
}

static act_t* get_act_for_address(size_t address) {
    /* Assuming images are contiguous, we want the greatest base less than address */
    size_t base = 0;
    size_t top_bits = 2;
    act_t* base_act = NULL;
	FOR_EACH_ACT(act) {
        size_t new_base = act->image_base;

        /* Should not confuse our two regions */

        if(new_base >> (64 - top_bits) == address >> (64 - top_bits)) {
            if(new_base <= address && new_base > base) {
                base = act->image_base;
                base_act = act;
            }
        }
    }}

    return base_act;
}

static act_t* get_act_for_pcc(capability pcc) {
    return get_act_for_address(cheri_getcursor(pcc));
}

static inline void print_frame(int num, capability ra) {
    if(cheri_getoffset(ra) > cheri_getlen(ra)) ra = cheri_setoffset(ra, 0);
    act_t* act = get_act_for_pcc(ra);
    size_t base = MIPS_KSEG0;
    char* name = "nano";
    if(act) {
        base = act->image_base;
        name = act->name;
    }

    size_t correct = correct_base(base, ra);
    // (sp=%p). PCC offset = 0x%016lx

    printf("%3d| [0x%016lx] in %16s.", num, correct, name);
}

static inline void print_frame_info(int16_t size, char* stack) {
    uint32_t on_stack = cheri_getlen(stack) - cheri_getoffset(stack);
    printf(" Frame size: %4x. Left On Stack: %4x. ", -size, on_stack);
}

static inline void print_change_stack(char* stack) {
    uint32_t on_stack = cheri_getlen(stack) - cheri_getoffset(stack);
    printf("\n Swap to new stack with size %4x\n", on_stack);
}

static inline void print_end(void) {
    printf("\n");
}

int check_cap(capability cap) {
	size_t perm = CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP;
	return ((cheri_getoffset(cap) >= cheri_getlen(cap)) ||
	((cheri_getperm(cap) & perm) != perm) ||
	(cap == NULL));
}


// TODO handle frames that are larger than the immediate field (need to decode a

void backtrace(char* stack_pointer, capability return_address, capability r17) {
	int i = 0;


	// Function prolog:
	// cincoffset $c11, $c11, -size // allocates space
	// ...
	// csc     c17,$zero,offset(c11)		// stores return address

    // Or a prolog with unsafe mem allocates space like this...
    // csc              $c11, $zero, -48($c10)
    // cincoffset       $c11, $c10, -size

    // Or a prolog with a particularly big frame
    // daddiu $1, $zero, -size
    // incoffset $c11, $c1_ , $1

	// Instruction Formats:

	// cincoffset i						// |010010|10011| cd  | cb  | im(11)
    // cincoffset                       // |010010|01101| cd  | cb  | rt  | (6) <- FIXME: LIES got from docs, is wrong

	// csc     cs,rt,im(cb)				// |111110|cs   |cb   |rt   |im(11)
	// csc     c17, $zero, offset(c11)  // |111110|10001|01011|00000|im(11)

	uint32_t cinc_c11_mask    = 0xFFFF0000;
	uint32_t csc_form_mask    = 0xFFFFF800;

	uint32_t cinci_form_val   = 0b0100101001101011U << 16U; // An inc offset immediate to c11
    uint32_t cinc_from_val    = 0x480b << 16U; // An inc offset to c11 // FIXME: Grabbed from dissassembler, not docs

	uint32_t csc_form_val	  = 0b111110100010101100000U << 11U;

	uint32_t cinc_i_mask 	  = (1 << 11) - 1;
    uint32_t cinc_cb_shift    = 11;
    uint32_t cinc_cb_mask     = (1 << 5) - 1;

	uint32_t csc_i_mask		  = (1 << 11) - 1;
    uint32_t daddiu_i_mask    = 0x0000FFFF;

	// FIXME maybe use frame pointer?

    int unsafe = 0;

	do {
		print_frame(i++, return_address);

        // This handles a branch out of the current function
		if(i == 1 && cheri_getoffset(return_address) >= cheri_getlen(return_address)) {
			return_address = cheri_setoffset(return_address, cheri_getlen(return_address)-4);
		}

		//scan backwards for cincoffset
		int16_t stack_size = 0;
		int16_t offset = 0;
		int found = 0;
		if(((size_t)return_address & 0xffffffff80000000) != 0xffffffff80000000) {
			for(uint32_t* instr = ((uint32_t*)return_address);; instr--) {
				if(check_cap(instr)) {
                    if(i == 1) break;
					printf("***bad frame (ran out of function)***\n");
					return;
				}
				uint32_t val = *instr;

                int cinc = (val & cinc_c11_mask) == cinc_from_val;
                int cinci = (val & cinc_c11_mask) == cinci_form_val;

                uint8_t from_reg = (uint8_t)((val >> cinc_cb_shift) & cinc_cb_mask);

                int safe_inc = (cinc || cinci) && (from_reg == 11);
                int unsafe_inc = (cinc || cinci) && (from_reg == 10);

                unsafe = unsafe_inc ? 1 : 0;

				if (safe_inc || unsafe_inc) {
                    if(cinci) {
                        stack_size = (int16_t)(val & cinc_i_mask);
                        if(stack_size & (1 << 10)) {
                            stack_size |= 0b11111 << 11;
                        } else stack_size = 0;
                    } else {
                        // FIXME: Just assume the previous instruction is the daddiu. Should probably check...
                        uint32_t prev_val = *(instr-1);
                        stack_size = (int16_t)(prev_val & daddiu_i_mask);
                    }
                    if(stack_size != 0) break;
				}

				if((val & csc_form_mask) == csc_form_val) {
					found = 1;
					offset = (int16_t)((val & csc_i_mask) << 4);
				}
			}
		} else if(i != 1) {
			printf("***address in nano kernel***\n");
			return;
		}

        print_frame_info(stack_size, stack_pointer);
		capability * ra_ptr = ((capability *)((stack_pointer + offset)));


		if((i == 1) && (found == 0)) {
            return_address = r17;
        } else {
            if(check_cap(ra_ptr)) {
                printf("***bad frame (ra bad)***\n");
                return;
            }

            return_address = *ra_ptr;
        }

		// Offset by 2 instructions for the cjal + nop
		return_address = (capability)((uint32_t*)return_address-2);
		stack_pointer = stack_pointer - stack_size;

        if(unsafe) {
            char** prev_stack_ptr = (char**)(stack_pointer - (3 * sizeof(capability)));
            if(check_cap(prev_stack_ptr)) {
                printf("*** bad frame (unsafe stack chain broken)***\n");
                return;
            }
            stack_pointer = *prev_stack_ptr;
            print_change_stack(stack_pointer);
        }

        print_end();

	} while(cheri_getoffset(stack_pointer) != cheri_getlen(stack_pointer));
	print_frame(i++, return_address);
    print_end();
}

static inline void dump_tlb() {

#define STRINGIFY(X) #X
#define ASM_MTCO(var, reg) "mtc0 %[" #var "], " STRINGIFY(reg) "\n"
#define ASM_MFCO(var, reg) "dmfc0 %[" #var "], " STRINGIFY(reg) "\n"

    register_t hi, lo0, lo1, pm, wired;

    __asm__ __volatile__(ASM_MFCO(W, MIPS_CP0_REG_WIRED):[W]"=r"(wired)::);

    printf("TLB status: \n\n");

    printf("|------------------------------------------------------------------------------|\n");
    printf("|        |        EntryHi      |         EntryLO0      |         EntryLO1      |\n");
    printf("|PageSize|---------------------|-----------------------|-----------------------|\n");
    printf("|        |  PAGE START  | ASID |      PFN0     |C|D|V|G|      PFN1     |C|D|V|G|\n");
    printf("|--------|--------------|------|---------------|-|-|-|-|---------------|-|-|-|-|\n");
    printf("|--------|--------------|------|---------------|-|-|-|-|---------------|-|-|-|-|%s\n", wired == 0 ? "<-wired" : "");
    for(int i = 0; i < N_TLB_ENTS; i++) {
        __asm__ __volatile__(
            ASM_MTCO(ndx, MIPS_CP0_REG_INDEX)
            "tlbr  \n"
            ASM_MFCO(HI, MIPS_CP0_REG_ENTRYHI)
            ASM_MFCO(LO0, MIPS_CP0_REG_ENTRYLO0)
            ASM_MFCO(LO1, MIPS_CP0_REG_ENTRYLO1)
            ASM_MFCO(PM, MIPS_CP0_REG_PAGEMASK)

        : [HI]"=r"(hi), [LO0]"=r"(lo0), [LO1]"=r"(lo1), [PM]"=r"(pm)
        : [ndx]"r"(i)
        :
        );

        register_t vpn = hi >> 13;
		vpn <<= UNTRANSLATED_BITS;
        register_t asid = hi & ((1 << 13) - 1);

        register_t pfn0 = lo0 >> 6;
        register_t c0 = (lo0 >> 3) & 0b111;
        register_t d0 = (lo0 >> 2) & 1;
        register_t v0 = (lo0 >> 1) & 1;
        register_t g0 = (lo0) & 1;

        register_t pfn1 = lo1 >> 6;
        register_t c1 = (lo1 >> 3) & 0b111;
        register_t d1 = (lo1 >> 2) & 1;
        register_t v1 = (lo1 >> 1) & 1;
        register_t g1 = (lo1) & 1;

        register_t pm_sz_k = (pm+1) * 4;
        int is_m = pm_sz_k >= 1024;
        if(is_m) pm_sz_k /= 1024;

        printf("|%4lx%sB*2|%14lx|  %2lx  |%15lx|%1lx|%1lx|%1lx|%1lx|%15lx|%1lx|%1lx|%1lx|%1lx|%s\n",
               pm_sz_k,is_m? "M" : "K", vpn, asid, pfn0, c0, d0, v0, g0,
            pfn1, c1, d1, v1, g1, i < wired ? "<-wired" : "");
    }

    printf("\n\n");
}

void kernel_dump_tlb(void) {
	capability all_powerfull = obtain_super_powers(); // Super magic wow!
	dump_tlb();
}

void regdump(int reg_num, act_t* kernel_curr_act) {
    // Note, dumping will not be possible when we enforce things properly
    // For now we use the obtain super powers to make it possible.
	if(kernel_curr_act == NULL) {
		kernel_curr_act = sched_get_current_act_in_pool(cp0_get_cpuid());
	}

    capability all_powerfull = obtain_super_powers(); // Super magic wow!

	int creg = 0;
	printf("Regdump:\n");
	CHERI_PRINT_CAP(kernel_curr_act->context);
	reg_frame_t* frame = cheri_incoffset(kernel_unseal_any(kernel_curr_act->context, all_powerfull), -0x3E0); // Magic number from asm.S
	CHERI_PRINT_CAP(frame);
	printf("Died in: %s\n", kernel_curr_act->name);
    dump_tlb();

	REG_DUMP_M(at, "($1)"); REG_DUMP_M(v0,"($2)"); REG_DUMP_M(v1,"($3)"); printf("\n");

	REG_DUMP_M(a0, "($4)"); REG_DUMP_M(a1, "($5)"); REG_DUMP_M(a2,"($6)"); REG_DUMP_M(a3,"($7)"); printf("\n");
	REG_DUMP_M(a4, "($8)"); REG_DUMP_M(a5, "($9)"); REG_DUMP_M(a6, "($10)"); REG_DUMP_M(a7, "($11)"); printf("\n");

	REG_DUMP_M(t0, "($12)"); REG_DUMP_M(t1, "($13)"); REG_DUMP_M(t2, "($14)"); REG_DUMP_M(t3, "($15)"); printf("\n");

	REG_DUMP_M(s0, "($16)"); REG_DUMP_M(s1, "($17)"); REG_DUMP_M(s2, "($18)"); REG_DUMP_M(s3, "($19)"); printf("\n");
	REG_DUMP_M(s4, "($20)"); REG_DUMP_M(s5, "($21)"); REG_DUMP_M(s6, "($22)"); REG_DUMP_M(s7, "($23)"); printf("\n");

	REG_DUMP_M(t8, "($24)"); REG_DUMP_M(t9, "($25)"); printf("\n");

	REG_DUMP_M(gp, "($28)"); REG_DUMP_M(sp, "($29)"); REG_DUMP_M(fp, "($30)"); REG_DUMP_M(ra, "($31)"); printf("\n");

	REG_DUMP_M(hi,""); REG_DUMP_M(lo,""); printf("\n");

	REG_DUMP_M(user_loc,""); printf("\n");

	printf("\n");

	REG_DUMP_C(default); printf("\n");

	REG_DUMP_C(c1); REG_DUMP_C(c2); printf("\n");

	REG_DUMP_C(c3); REG_DUMP_C(c4); REG_DUMP_C(c5); REG_DUMP_C(c6);  printf("\n");
	REG_DUMP_C(c7); REG_DUMP_C(c8); REG_DUMP_C(c9); REG_DUMP_C(c10); printf("\n");

	REG_DUMP_C(c11); REG_DUMP_C(c12); REG_DUMP_C(c13);
	REG_DUMP_C(c14); REG_DUMP_C(c15); printf("\n");

	REG_DUMP_C(c16); REG_DUMP_C(c17); printf("\n");

	REG_DUMP_C(c18); REG_DUMP_C(c19); REG_DUMP_C(c20); REG_DUMP_C(c21); printf("\n");
	REG_DUMP_C(c22); REG_DUMP_C(c23); REG_DUMP_C(c24); REG_DUMP_C(c25); printf("\n");

    register_t cause;
    __asm__ ("mfc0 %[c], $13" : [c]"=r"(cause) ::);

	REG_DUMP_C(idc); creg = 31; REG_DUMP_C_extra(pcc, cause & (1 << 31) ? " (delay slot)" : ""); printf("\n");

	printf("\nLoaded images:\n");
	FOR_EACH_ACT(act) {
		printf("%16s: %lx\n", act->name, act->image_base);
	}}
    printf("%16s: %lx\n", "nano", MIPS_KSEG0);

	printf("\nAttempting backtrace:\n\n");
	char * stack_pointer = (char*)frame->cf_c11;
	capability return_address = frame->cf_pcc;
	backtrace(stack_pointer, return_address, frame->cf_c17);
}

#endif
