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


#include "exceptions.h"
#include "nano/usernano.h"
#include "dylink.h"
#include "assert.h"
#include "exception_cause.h"

__thread int trampoline_registered = 0;

handler_t* handle_vector[MIPS_CP0_EXCODE_NUM];
handler_t* chandle_vector[CAP_CAUSE_NUM];

#ifdef USE_EXCEPTION_STACK
// Really want an annotation to set offset for this, but it doesn't exit. Will hack it in assembly in register.
__thread capability exception_stack[EXCEPTION_STACK_SIZE/sizeof(capability)];
#endif
#ifdef USE_EXCEPTION_UNSAFE_STACK
__thread capability unsafe_exception_stack[EXCEPTION_UNSAFE_STACK_SIZE/sizeof(capability)];
#endif


void register_vectored_exception(handler_t* handler, register_t excode) {
    assert(excode < MIPS_CP0_EXCODE_NUM);

    handle_vector[excode] = handler;
    register_exception_raw(&user_exception_trampoline_vector, get_idc());
}

void register_vectored_exception2(handler2_t* handler, register_t excode) {
    assert(excode < MIPS_CP0_EXCODE_NUM);

    handle_vector[excode] = (handler_t*)((char*)(handler)+1);
    register_exception_raw(&user_exception_trampoline_vector, get_idc());
}

void register_vectored_cap_exception(handler_t* handler, register_t excode) {
    assert(excode < CAP_CAUSE_NUM);

    chandle_vector[excode] = handler;
    register_exception_raw(&user_exception_trampoline_vector, get_idc());
}

void register_exception_raw(ex_pcc_t* exception_pcc, capability exception_idc) {
    get_ctl()->ex_pcc = exception_pcc;
    get_ctl()->ex_idc = exception_idc;

    if(trampoline_registered == 0) {
        // Dont know how to get relocations with offsets in C so...
#define INC_STACK(SN, I)    \
        __asm__ (\
                "clcbi  $c1, %%captab_tls20("SN")($c26)      \n"\
                "li     $t0, %[im]                                      \n"\
                "cincoffset $c1, $c1, $t0                               \n"\
                "cscbi  $c1, %%captab_tls20("SN")($c26)      \n"\
                    :\
                    : [im]"i"(I)\
                    : "t0", "$c1"\
                )
#ifdef USE_EXCEPTION_STACK
        INC_STACK("exception_stack", EXCEPTION_STACK_SIZE);
#endif
#ifdef USE_EXCEPTION_UNSAFE_STACK
        INC_STACK("unsafe_exception_stack", EXCEPTION_UNSAFE_STACK_SIZE);
#endif
        exception_subscribe();
        trampoline_registered = 1;
    }
}
