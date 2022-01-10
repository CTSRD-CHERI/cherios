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

#ifndef CHERIOS_REGDUMP_H
#define CHERIOS_REGDUMP_H

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
    size_t base = NANO_KSEG;
    const char* name = "nano";
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

static inline void print_change_stack(char* stack, const char* reason) {
    uint32_t on_stack = cheri_getlen(stack) - cheri_getoffset(stack);
    printf("\n Swap to new stack with size %4x due to %s", on_stack, reason);
}

static inline void print_end(void) {
    printf("\n");
}

static int check_cap(capability cap) {
    size_t perm = CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP;
    return ((cheri_getoffset(cap) >= cheri_getlen(cap)) ||
            ((cheri_getperm(cap) & perm) != perm) ||
            (cap == NULL));
}

#endif //CHERIOS_REGDUMP_H
