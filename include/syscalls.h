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
#include "cheriplt.h"

// FIXME as my understanding of how to use ccall has evolved I realise this has been done incorrectly
// FIXME the data capability for a ccall should contain c0/a stack/a capability to unseal any arguments
// FIXME therefore something like message send should have the target activation as an EXPLICIT argument
// FIXME this will solve many issues. It has been done properly for the nano kernel

#define SYS_CALL_LIST(ITEM, ...)                                                                                   \
        ITEM(message_send, register_t, (register_t a0, register_t a1, register_t a2,                               \
                                        const_capability c3, const_capability c4, const_capability c5,             \
                                        register_t selector, register_t v0), __VA_ARGS__)                                    \
        ITEM(message_reply, int, (capability c3, capability sync_token, register_t v0, register_t v1), __VA_ARGS__)          \
        ITEM(sleep, void, (int time), __VA_ARGS__)                                                                           \
        ITEM(wait, void, (void), __VA_ARGS__)                                                                                \
        ITEM(syscall_act_register, act_control_kt, (reg_frame_t * frame, const char * name, queue_t * queue, register_t a0), __VA_ARGS__)  \
        ITEM(syscall_act_ctrl_get_ref, act_kt, (void), __VA_ARGS__)                                                          \
        ITEM(syscall_act_ctrl_get_status, status_e, (void), __VA_ARGS__)                                                     \
        ITEM(syscall_act_revoke, int, (void), __VA_ARGS__)                                                                   \
        ITEM(syscall_act_terminate, int, (void), __VA_ARGS__)                                                                \
        ITEM(syscall_puts, void, (const char* msg), __VA_ARGS__)                                                             \
        ITEM(syscall_panic, void, (void), __VA_ARGS__)                                                                       \
        ITEM(syscall_interrupt_register, int, (int number), __VA_ARGS__)                                                     \
        ITEM(syscall_interrupt_enable, int, (int number), __VA_ARGS__)                                                       \
        ITEM(syscall_gc,int, (capability p, capability pool), __VA_ARGS__)                                                   \

#define CCALL_SELECTOR_LIST(ITEM)   \
        ITEM(SEND,1)                \
        ITEM(SEND_SWITCH,2)         \
        ITEM(SYNC_CALL,4)           \

DECLARE_ENUM(ccall_selector_t, CCALL_SELECTOR_LIST)

#ifndef __ASSEMBLY__

#include "types.h"
#include "queue.h"

        #define SYSCALL_OBJ(call, obj, ...) call ## _inst (CONTEXT(kernel_if.call, obj),  __VA_ARGS__ )
        #define SYSCALL_OBJ_void(call, obj) call ## _inst (CONTEXT(kernel_if.call, obj))

#endif

PLT(kernel_if_t, SYS_CALL_LIST)

#endif //CHERIOS_SYSCALLS_H
