/*-
 * Copyright (c) 2017 Lawrence Esswood
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

#ifndef CHERIOS_SYSCALLS_H
#define CHERIOS_SYSCALLS_H

#include "string_enums.h"

#define SYS_CALL_LIST(ITEM)                                                                                     \
        ITEM(message_send, register_t, (register_t a0, register_t a1, register_t a2,                            \
                                        const_capability c3, const_capability c4, const_capability c5,          \
                                        register_t selector, register_t v0))                                    \
        ITEM(message_reply, int, (capability c3, capability sync_token, register_t v0, register_t v1))          \
        ITEM(sleep, void, (int time))                                                                           \
        ITEM(wait, void, (void))                                                                                \
        ITEM(syscall_act_register, act_control_kt, (reg_frame_t * frame, const char * name, queue_t * queue, register_t a0))  \
        ITEM(syscall_act_ctrl_get_ref, act_kt, (void))                                                          \
        ITEM(syscall_act_ctrl_get_status, status_e, (void))                                                     \
        ITEM(syscall_act_revoke, int, (void))                                                                   \
        ITEM(syscall_act_terminate, int, (void))                                                                \
        ITEM(syscall_puts, void, (const char* msg))                                                             \
        ITEM(syscall_panic, void, (void))                                                                       \
        ITEM(syscall_interrupt_register, int, (int number))                                                     \
        ITEM(syscall_interrupt_enable, int, (int number))                                                       \
        ITEM(syscall_gc,int, (capability p, capability pool))                                                   \

#define CCALL_SELECTOR_LIST(ITEM)   \
        ITEM(SEND,1)                \
        ITEM(SEND_SWITCH,2)         \
        ITEM(SYNC_CALL,4)           \

DECLARE_ENUM(ccall_selector_t, CCALL_SELECTOR_LIST)

#ifndef __ASSEMBLY__

        #include "cheric.h"
        #include "ccall.h"
        #include "types.h"
        #include "queue.h"

        //TODO once we have proper linker support, we won't have to manually specify the interface like this

        #define PLT_GOT_ENTRY(name, ret, sig) capability name;

        typedef struct
        {
            SYS_CALL_LIST(PLT_GOT_ENTRY)
        } kernel_if_t;

        struct cheri_object default_obj;

        #define CCALL_WRAP(name, ret, sig)                                              \
                __attribute__((cheri_ccall))                                            \
                __attribute__((cheri_method_suffix("_inst")))                           \
                __attribute__((cheri_method_class(name ## _ ## default_obj)))           \
                ret name sig;

        #define MAKE_DEFAULT(name, ret, sig) struct cheri_object name ## _ ## default_obj;

        SYS_CALL_LIST(MAKE_DEFAULT)

        SYS_CALL_LIST(CCALL_WRAP)

        #undef CCALL_WRAP
        #undef MAKE_DEFAULT
        #undef PLT_GOT_ENTRY


#define SYSCALL_OBJ(call, obj, ...) call ## _inst (CONTEXT(kernel_if.call, obj),  __VA_ARGS__ )
#define SYSCALL_OBJ_void(call, obj) call ## _inst (CONTEXT(kernel_if.call, obj))

#else // __ASSEMBLY__

        .set enum_ctr, 0
        #define SET_NAME(NAME, ret, sig) .set NAME ## _offset, (enum_ctr * CAP_SIZE); .set enum_ctr, enum_ctr + 1;
        SYS_CALL_LIST(SET_NAME)

#endif



#endif //CHERIOS_SYSCALLS_H
