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

#define MinStackSize    0x2000 // The compiler throws away stacks smaller than this!

int temporal_exception_handle(register_t cause, register_t ccause, exception_restore_frame* restore_frame) {
// Looking for: csetbounds     $c15, $c15, MinStackSize  <-- will fail if too small or non existant

    // TODO
    uint32_t mask = 0; // If we don't care exactly what the immediate/reg/exact instruction is

    uint32_t handle_instr = 0x480f7848;
    uint32_t * epcc = (uint32_t*)get_ctl()->ex_pcc;

    if(!((cheri_getoffset(epcc) >= 0) && (cheri_getoffset(epcc) < cheri_getlen(epcc)))) return 1;

    uint32_t fault_instr = *(epcc) &~mask;

    // We might fail because we have no unsafe stack, or because its not large enough to use
    if(handle_instr != fault_instr) return 1;

    capability old_c10 = cheri_getreg(10);
    uint64_t default_unsafe_stack_size = MinStackSize + UNTRANSLATED_PAGE_SIZE + MEM_REQUEST_MIN_REQUEST;

    if(old_c10 != NULL) {
        mem_release(cheri_getbase(old_c10), default_unsafe_stack_size, 1, own_mop);
    }

    ERROR_T(res_t) stack_res = mem_request(0, default_unsafe_stack_size, 0, own_mop);
    //if(!IS_VALID(stack_res)) return 1; // We failed to get a new stack

    cap_pair pair;
    rescap_take(stack_res.val, &pair);

    capability new_c10 = pair.data;

    new_c10 = cheri_setoffset(new_c10, default_unsafe_stack_size);
    restore_frame->c15 = new_c10 - 0x2000;
    cheri_setreg(10, new_c10);

    return 0;
}
