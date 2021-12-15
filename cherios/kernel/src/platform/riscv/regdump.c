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
#include "regdump.h"
#define printf kernel_printf

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

    backtrace(frame->cf_c2, frame->cf_pcc, frame->cf_idc, frame->cf_c1, NULL);
    (void)reg_num;
}

void backtrace(char* stack_pointer, capability return_address, capability idc, capability r17, capability c18) {
    (void)stack_pointer;
    (void)return_address;
    (void)idc;
    (void)r17;
    (void)c18;

    /*
    capability code_power;
    capability data_power = obtain_super_powers(&code_power);
    */

    // Mostly TODO
    act_t* act = get_act_for_address((size_t)return_address);
    size_t base = correct_base(act ? act->image_base : 0, return_address);
    printf("Backtrace:\n");
    printf("In %s\n offset %lx\n", act ? act->name : "unknown", base);
}
