/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lawrence Esswood
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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
#include "cpu.h"
#define printf kernel_printf
#include "regdump.h"

// TODO RISCV

void kernel_dump_tlb(void) {

}

void regdump_printcap(const char* name, void* cap) {
    signed long type = cheri_gettype(cap);
    const char* vals[] = {NULL, NULL,"(sentry)","(?3)"};
    const char* str = NULL;
    if(type >= 0) {
        str = "(user)";
    } else if(type < 0) {
        type = -type;
        str = type < 3 ? vals[type] : "(?large)";
    }

    kernel_printf("%4s: %016lx",
            name,
            cheri_getcursor(cap));

    if(str || cheri_gettag(cap) || cheri_getbase(cap) != 0 || cheri_gettop(cap) != (unsigned long)~0) {
        kernel_printf(" | t:%d B:%016lx T:%016lx",
                    (int)cheri_gettag(cap),
                    cheri_getbase(cap),
                    cheri_gettop(cap));
    }

    if (str) {
        kernel_printf(" type:%lx%s\n", type, str);
    } else {
        kernel_printf("\n");
    }
}

void regdump(int reg_num, act_t* act) {
    if(!act) {
        act = sched_get_current_act_in_pool(cpu_get_cpuid());
    }
    capability code_power;
    capability data_power = obtain_super_powers(&code_power);

    kernel_printf("Regdump:\n");
    CHERI_PRINT_CAP(act->context);
    reg_frame_t* frame = kernel_unseal_any(act->context, data_power);
    CHERI_PRINT_CAP(frame);

#define DUMP_REG(a, b) regdump_printcap(#b, frame->a)

    DUMP_REG(cf_default, ddc);
    DUMP_REG(cf_pcc, pcc);

    DUMP_REG(cf_c1, cra);
    DUMP_REG(cf_c2, csp);
    DUMP_REG(cf_c3, cgp);
    DUMP_REG(cf_c4, cusp);
    DUMP_REG(cf_c5, ct0);
    DUMP_REG(cf_c6, ct1);
    DUMP_REG(cf_c7, ct2);
    DUMP_REG(cf_c8, cs0);
    DUMP_REG(cf_c9, cs1);
    DUMP_REG(cf_c10, ca0);
    DUMP_REG(cf_c11, ca1);
    DUMP_REG(cf_c12, ca2);
    DUMP_REG(cf_c13, ca3);
    DUMP_REG(cf_c14, ca4);
    DUMP_REG(cf_c15, ca5);
    DUMP_REG(cf_c16, ca6);
    DUMP_REG(cf_c17, ca7);
    DUMP_REG(cf_c18, cs2);
    DUMP_REG(cf_c19, cs3);
    DUMP_REG(cf_c20, cs4);
    DUMP_REG(cf_c21, cs5);
    DUMP_REG(cf_c22, cs6);
    DUMP_REG(cf_c23, cs7);
    DUMP_REG(cf_c24, cs8);
    DUMP_REG(cf_c25, cs9);
    DUMP_REG(cf_c26, cs10);
    DUMP_REG(cf_c27, cs11);
    DUMP_REG(cf_c28, ct3);
    DUMP_REG(cf_c29, ct4);
    DUMP_REG(cf_c30, ct5);
    DUMP_REG(cf_c31, idc);

    backtrace(frame->cf_c2, frame->cf_pcc, frame->cf_idc, frame->cf_c1, frame->cf_c8);
    (void)reg_num;
}

void* rederive(capability cap, capability from) {
    if (!cheri_gettag(cap))
        return cap;
    capability result = from;
    result = cheri_setcursor(result, cheri_getbase(cap));
    result = cheri_setbounds(result, cheri_getlen(cap));
    result = cheri_setcursor(result, cheri_getcursor(cap));
    return result;
}

// Needs renaming across mips/riscv. Here its backtrace(sp, pc, idc, ra, fp)
// TODO: Not working because removing -fomit-frame-pointer does not seem to get frame pointer back in
void backtrace(char* stack_pointer, capability return_address, capability idc, capability r17, capability c18) {

    capability all_power_pcc;
    capability all_power = obtain_super_powers(&all_power_pcc);

    (void)all_power;
    (void)idc;

    int frame_idx = 0;

    return_address = rederive(return_address, all_power_pcc);
    char* frame_pointer = (char*)c18;

    while(cheri_getoffset(frame_pointer) != cheri_getlen(frame_pointer)) {
        print_frame(frame_idx++, return_address);
        if (frame_pointer == stack_pointer && frame_idx == 1) {
            return_address = r17;
        } else {
            stack_pointer = frame_pointer;
            return_address = ((capability*)stack_pointer)[-1];
            frame_pointer = ((capability*)stack_pointer)[-2];
        }
    }

    print_frame(frame_idx, return_address);
    print_end();
}
