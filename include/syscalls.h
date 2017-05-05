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

#ifndef __ASSEMBLY__

#define SYSCALL(NUM) __asm__ __volatile__ ("li $v0, %[num] \n" "syscall \n":: [num]"I" (NUM): "v0")

#define SYSCALL_a0_base(NUM, _a0, EXTRA, OUTS) \
__asm__ __volatile__ (                       \
        "li $v0, %[num] \n"                  \
        "move $a0, %[a0]\n"                  \
        "syscall \n"                         \
        EXTRA                                \
        : OUTS                               \
        : [num]"I" (NUM), [a0]"r" (_a0)      \
        : "v0", "a0")                        \

#define SYSCALL_c3_base(NUM, _c3, EXTRA, OUTS) \
__asm__ __volatile__ (                       \
        "li $v0, %[num] \n"                  \
        "cmove $c3, %[c3]\n"                 \
        "syscall \n"                         \
        EXTRA                                \
        : OUTS                               \
        : [num]"I" (NUM), [c3]"r" (_c3)      \
        : "v0", "$c3")                       \

#define SYSCALL_a0(NUM, _a0) SYSCALL_a0_base(NUM, _a0,,)
#define SYSCALL_a0_retr(NUM, _a0, _ret) SYSCALL_a0_base(NUM, _a0, "move %[ret], $v0\n", [ret] "=r" (_ret))
#define SYSCALL_a0_retc(NUM, _a0, _ret) SYSCALL_a0_base(NUM, _a0, "cmove %[ret], $c3\n", [ret] "=C" (_ret))

#define SYSCALL_c3(NUM, _c3) SYSCALL_c3_base(NUM, _c3,,)
#define SYSCALL_c3_retr(NUM, _c3, _ret) SYSCALL_c3_base(NUM, _c3, "move %[ret], $v0\n", [ret] "=r" (_ret))
#define SYSCALL_c3_retc(NUM, _c3, _ret) SYSCALL_c3_base(NUM, _c3, "cmove %[ret], $c3\n", [ret] "=C" (_ret))

#endif // __ASSEMBLY__

#define SYS_CALL_LIST(ITEM)           \
        ITEM(SLEEP,13)                \
        ITEM(WAIT,14)                 \
        ITEM(ACT_REGISTER,20)         \
        ITEM(ACT_CTRL_GET_REF,21)     \
        ITEM(ACT_CTRL_GET_STATUS,23)  \
        ITEM(ACT_REVOKE,25)           \
        ITEM(ACT_TERMINATE,26)        \
        ITEM(PUTS,34)                 \
        ITEM(PANIC,42)                \
        ITEM(INTERRUPT_REGISTER,50)   \
        ITEM(INTERRUPT_ENABLE,51)     \
        ITEM(GC,66)                   \

DECLARE_ENUM(syscalls_t, SYS_CALL_LIST)

#define CCALL_SELECTOR_LIST(ITEM)   \
        ITEM(SEND,1)                \
        ITEM(SEND_SWITCH,2)         \
        ITEM(SYNC_CALL,4)           \

DECLARE_ENUM(ccall_selector_t, CCALL_SELECTOR_LIST)

#endif //CHERIOS_SYSCALLS_H
