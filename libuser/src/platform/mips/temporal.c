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

#include "temporal.h"

// A function that does nothing but perform the check sequence we use for unsafe stacks then returns
__asm (
SANE_ASM
".text; .global try_replace_usp; .ent try_replace_usp; .hidden try_replace_usp; try_replace_usp: \n"
"cgetoffset      $1, $c10     \n"
"tltiu           $1, 0x4000   \n"
"ccall           $c17, $c18, 2\n"
"nop                          \n"
".end try_replace_usp         \n"
);


// Consumes the entire unsafe stack so on the next use it will fault and be replaced
void consume_usp(void) {
    //
    __asm __volatile (SANE_ASM
    "cbez       $c10, 1f                \n"
    "nop                                \n"
    "clcbi      $c1, %[off]($c10)       \n"
    "li         $t0, -%[off]            \n"
    "csetoffset $c10, $c10, $t0         \n"
    "cscbi       $c1, %[off]($c10)      \n"
    "1:\n"
    :
    : [off]"i"(CSP_OFF_NEXT)
    :"$c10", "$c1", "t0");
}

#define REG_MASK            0b11111

#define CGETOFFSET              0b01001000000000000101000000000111
#define CGETOFFSET_CHECK_MASK   0b11111111111000001111100000000111
#define CGETOFFSET_REG_SHIFT    16


#define TLTIU                   0b00000100000010110000000000000000
#define TLTIU_CHECK_MASK        0b11111100000111110000000000000000
#define TLTIU_REG_SHIFT         21
#define TLTIU_IM_MASK           0xFFFF

int temporal_check_insts(uint32_t fault_instr, uint32_t prev_fault_instr) {
    // Check instructions are what we are looking for

    if(!(((fault_instr & TLTIU_CHECK_MASK) == TLTIU) && ((prev_fault_instr & CGETOFFSET_CHECK_MASK) == CGETOFFSET)))
        return 1;

    // Check they use the same reg
    if(((fault_instr >> TLTIU_REG_SHIFT) ^ (prev_fault_instr >> CGETOFFSET_REG_SHIFT)) & REG_MASK)
        return 1;

    return 0;
}
