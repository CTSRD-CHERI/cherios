/*-
 * Copyright (c) 2018 Lawrence Esswood
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

#include <exceptions.h>
#include "cheric.h"
#include "exceptions.h"
#include "stdlib.h"
#include "mman.h"
#include "temporal.h"

// WARN: It is a _really_ bad idea to call a function that will use too much unsafe stack from here.
// WARN: We can cope with exactly 1 use of the unsafe stack (enough for secure loaded things to make a call out)


#if(EXTRA_TEMPORAL_TRACKING && !LIGHTWEIGHT_OBJECT)
    size_t stack_bases[EXTRA_TEMPORAL_TRACKING];
    size_t at_stacks = 1; // We will start with 1 stack. It should never have to be replaced.


    static inline void made_new_stack(capability new, capability old) {
        size_t old_base = (size_t)cheri_getbase(old);
        size_t new_base = (size_t)cheri_getbase(new);

        size_t level = 0;

        if(old_base == 0) {
            // This is a stack at a new level
            level = at_stacks++;
        } else {
            for(size_t i = 0; i != at_stacks; i++) {
                if(stack_bases[i] == old_base) {
                    level = i;
                    break;
                }
            }
        }

        assert(level != 0);

        stack_bases[level] = new_base;

        if(own_stats) own_stats->stacks_at_level[level]++;
    }

#endif



capability new_stack(capability old_c10) {

    assert(cheri_getlen(old_c10) != EXCEPTION_UNSAFE_STACK_SIZE);

    uint64_t default_unsafe_stack_size = NewTemporalStackSize;

    if(old_c10 != NULL) {
        assert(cheri_getoffset(old_c10) < MinStackSize);
        mem_release(cheri_getbase(old_c10), default_unsafe_stack_size, 1, own_mop);
    }

    // FIXME: mem_request will consume some of the temporal unsafe stack
    // FIXME: we should give ourselves a new one if it looks like its running out
    ERROR_T(res_t) stack_res = mem_request(0, default_unsafe_stack_size, EXACT_SIZE | COMMIT_NOW | REPRESENTABLE, own_mop);
    // if(!IS_VALID(stack_res)) return 1; // We failed to get a new stack

    if(!IS_VALID(stack_res)) {
        HW_TRACE_ON;
        __asm __volatile("move $v0, %[er]   \n"::[er]"r"((ssize_t)stack_res.er):"v0");
        HW_TRACE_OFF;
    }
    assert(IS_VALID(stack_res));

#if !(LIGHTWEIGHT_OBJECT)
    if(own_stats) own_stats->temporal_reqs++;
#endif

    _safe cap_pair pair;
    rescap_take(stack_res.val, &pair);

    capability new_c10 = pair.data;

    new_c10 = cheri_setoffset(new_c10, default_unsafe_stack_size);

    return new_c10;
}

int temporal_exception_handle(__unused register_t cause, __unused register_t ccause, exception_restore_frame* restore_frame) {
// Looking for: cgetoffset $X, $c10; tltiu $X, Y

#if(UNSAFE_STACKS_OFF)
    return 1;
#endif

#define REG_MASK            0b11111

#define CGETOFFSET              0b01001000000000000101000000000111
#define CGETOFFSET_CHECK_MASK   0b11111111111000001111100000000111
#define CGETOFFSET_REG_SHIFT    16


#define TLTIU                   0b00000100000010110000000000000000
#define TLTIU_CHECK_MASK        0b11111100000111110000000000000000
#define TLTIU_REG_SHIFT         21
#define TLTIU_IM_MASK           0xFFFF


    uint32_t * epcc = (uint32_t*)get_ctl()->ex_pcc;

    if(!((cheri_getoffset(epcc) >= 4) && (cheri_getoffset(epcc) < cheri_getlen(epcc)))) return 1;

    uint32_t fault_instr = *(epcc);
    uint32_t prev_fault_instr = *(epcc-1);

    // Check instructions are what we are looking for

    if(!(((fault_instr & TLTIU_CHECK_MASK) == TLTIU) && ((prev_fault_instr & CGETOFFSET_CHECK_MASK) == CGETOFFSET)))
        return 1;

    // Check they use the same reg
    if(((fault_instr >> TLTIU_REG_SHIFT) ^ (prev_fault_instr >> CGETOFFSET_REG_SHIFT)) & REG_MASK)
        return 1;

    // Get the immediate
    // uint16_t user_wants_size = (uint16_t)(fault_instr & TLTIU_IM_MASK);

    // TODO We really should provide a stack with max(Y,MinStackSize) + extra. But for now just give MinStackSize + Extra

    capability old_c10;

#ifdef USE_EXCEPTION_UNSAFE_STACK
    old_c10 = restore_frame->c10;
#else
    old_c10 = cheri_getreg(10);
#endif

    capability old_link = NULL;

    if(old_c10) {
        old_link = *(capability *)(((char*)old_c10) + CSP_OFF_NEXT);
    }

#if !(LIGHTWEIGHT_OBJECT)
    if(!old_c10 && own_stats) own_stats->temporal_depth++;
#endif

    __thread static int replaced_first_c10 = 0;
    int swap_back = 0;

    // Performing the check in the untrusting stub leads to a catch 22. This is very much a hack.

    capability mode = init_kernel_if_t_get_mode();
    if(replaced_first_c10 == 0 && mode == (capability)&plt_common_untrusting) {
        // stop the second exception from firing due very small stack
        init_kernel_if_t_change_mode((char*)mode + 8); // skip the 2 check instructions
        swap_back = 1;
    }

    capability new_c10 = new_stack(old_c10);

#if(EXTRA_TEMPORAL_TRACKING && !LIGHTWEIGHT_OBJECT)
    made_new_stack(new_c10, old_c10);
#endif

    if(old_link) {
        *(capability *)(((char*)new_c10) + CSP_OFF_NEXT) = old_link;
    }
    get_ctl()->ex_pcc = (ex_pcc_t*)(((char*)epcc) + 4);

#ifdef USE_EXCEPTION_UNSAFE_STACK
    restore_frame->c10 = new_c10;

    capability  old_ex_c10 = cheri_getreg(10);

    if(replaced_first_c10 == 0 || cheri_getoffset(old_ex_c10) < MinStackSize) {
        capability new_ex_c10 = new_stack(replaced_first_c10 ? old_ex_c10 : NULL);
        cheri_setreg(10, new_ex_c10);

        if(swap_back) {
            // Exception stack now the right size
            init_kernel_if_t_change_mode((char*)init_kernel_if_t_get_mode() - 8); // skip the 2 check instructions
        }

        replaced_first_c10 = 1;
    }

    // The cusp to use on cross domain call may get clobbered by getting a new stack (e.g., by message send)
    // The temporal unsafe stack should never join the chain of the stack proper
    // This exception handler may get called from a cross domain stub, where the ctl->cusp matters

    // The correct value to set it to is the old link
    ((CTL_t*)(get_ctl()->ex_idc))->cusp = old_link;
#else
    cheri_setreg(10, new_c10);
#endif

    return 0;
}

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

void replace_usp(void) {
    consume_usp();
    try_replace_usp();
}
