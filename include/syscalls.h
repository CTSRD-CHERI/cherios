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
#include "nano/nanotypes.h"


#if (GO_FAST)
        #define GO_FAST_ITEMS(ITEM, ...)
        #define syscall_puts(s) ((void)s)
#else
        #define GO_FAST_ITEMS(ITEM, ...)\
                        ITEM(syscall_puts, void, (const char* msg), __VA_ARGS__)
#endif

#define SYS_CALL_LIST(ITEM, ...)                                                                                   \
        ITEM(message_send, register_t, (register_t a0, register_t a1, register_t a2, register_t,                   \
                                        const_capability c3, const_capability c4, const_capability c5, const_capability c6,     \
                                        act_kt dest, register_t selector, register_t v0), __VA_ARGS__,                          \
                                         ".global message_send_c \n message_send_c: \n", "message_send_c_end: .size message_send_c, message_send_c_end-message_send_c\n") \
        ITEM(message_reply, int, (capability c3, register_t v0, register_t v1, act_reply_kt caller, int hint_switch), __VA_ARGS__)          \
/* In mips clock ticks*/\
        ITEM(sleep, void, (register_t timeout), __VA_ARGS__)                                                                           \
        ITEM(wait, void, (void), __VA_ARGS__)\
/*Fastpath wait should not be called from C. Its API is detailed in msg.S*/\
        ITEM(fastpath_wait, void, (capability c3, register_t v0, register_t v1, act_reply_kt reply_token, int64_t timeout, int notify_is_timeout), __VA_ARGS__)\
        ITEM(syscall_act_register, act_control_kt, (reg_frame_t * frame, const char * name, queue_t * queue, res_t res, uint8_t cpu_hint), __VA_ARGS__)\
        ITEM(syscall_act_ctrl_get_ref, act_kt, (act_control_kt ctrl), __VA_ARGS__)                                                          \
        ITEM(syscall_act_ctrl_get_status, status_e, (act_control_kt ctrl), __VA_ARGS__)                                      \
        ITEM(syscall_act_ctrl_get_sched_status, sched_status_e, (act_control_kt ctrl), __VA_ARGS__)                          \
        ITEM(syscall_act_revoke, int, (act_control_kt ctrl), __VA_ARGS__)                                                    \
        ITEM(syscall_act_terminate, int, (act_control_kt ctrl), __VA_ARGS__)                                                 \
        GO_FAST_ITEMS(ITEM, __VA_ARGS__)\
        ITEM(syscall_panic, void, (void), __VA_ARGS__)                                                                       \
        ITEM(syscall_panic_proxy, void, (act_kt proxy), __VA_ARGS__)                                                                       \
        ITEM(syscall_interrupt_register, int, (int number, act_control_kt ctrl, register_t v0, register_t arg, capability carg), __VA_ARGS__)                                                     \
        ITEM(syscall_interrupt_enable, int, (int number, act_control_kt ctrl), __VA_ARGS__)                                  \
        ITEM(syscall_shutdown, void, (shutdown_t), __VA_ARGS__)                                                              \
        ITEM(syscall_register_act_event_registrar, void, (act_kt act), __VA_ARGS__)                                          \
        ITEM(syscall_get_name, const char*, (act_kt), __VA_ARGS__)\
        ITEM(syscall_act_ctrl_get_notify_ref, act_notify_kt, (act_control_kt ctrl), __VA_ARGS__)\
/* Duration of sleep is returned */\
        ITEM(syscall_cond_wait, register_t, (int notify_on_message, register_t timeout), __VA_ARGS__)\
        ITEM(syscall_cond_notify, void, (act_notify_kt waiter), __VA_ARGS__)\
        ITEM(syscall_cond_cancel, void, (void), __VA_ARGS__)\
        ITEM(syscall_now, register_t, (void), __VA_ARGS__)\
        ITEM(syscall_vmem_notify, void, (act_notify_kt waiter, int suggest_switch), __VA_ARGS__)\
        ITEM(syscall_change_priority, void, (act_control_kt ctrl, enum sched_prio priority), __VA_ARGS__)\
        ITEM(syscall_dump_tlb, void, (void), __VA_ARGS__)\
        ITEM(syscall_provide_sync, size_t, (res_t res), __VA_ARGS__)\
        ITEM(syscall_next_sync, void, (void), __VA_ARGS__)\
/* Some quesitonable syscalls i'm using for debugging */\
        ITEM(syscall_actlist_first, act_control_kt, (void), __VA_ARGS__)\
        ITEM(syscall_actlist_next, act_control_kt, (act_control_kt act), __VA_ARGS__)\
        ITEM(syscall_act_info, void, (act_control_kt act, act_info_t* info), __VA_ARGS__)\
        ITEM(syscall_act_user_info_ref, user_stats_t*, (act_control_kt act), __VA_ARGS__)\
        ITEM(syscall_info_epoch, void, (void), __VA_ARGS__)\
        ITEM(syscall_bench_start, uint64_t, (void), __VA_ARGS__)\
        ITEM(syscall_bench_end, uint64_t, (void), __VA_ARGS__)\
        ITEM(syscall_hang_debug, void, (void), __VA_ARGS__)

// Found by trial and error
#ifdef HARDWARE_qemu
        #define CLOCK_TO_US(X)  ((uint64_t)((X) >> 5))
        #define CLOCK_TO_MS(X) (uint64_t)(X >> 15)
        #define MS_TO_CLOCK(X) (((uint64_t)X) << 15)
#else
        #define CLOCK_TO_US(X)  ((uint64_t)((X) >> 6))
        #define CLOCK_TO_MS(X) (uint32_t)(X >> 16)
        #define MS_TO_CLOCK(X) (((uint64_t)X) << 16)
#endif

#define CCALL_SELECTOR_LIST(ITEM)   \
        ITEM(SEND,1)                \
        ITEM(SEND_SWITCH,2)         \
        ITEM(SYNC_CALL,4)           \

DECLARE_ENUM(ccall_selector_t, CCALL_SELECTOR_LIST)

#define SHUTDOWN_TYPES_LIST(ITEM)   \
        ITEM(SHUTDOWN, 0)           \
        ITEM(REBOOT, 1)

DECLARE_ENUM(shutdown_t, SHUTDOWN_TYPES_LIST)
#ifndef __ASSEMBLY__

#include "types.h"
#include "queue.h"

#endif

PLT_thr(kernel_if_t, SYS_CALL_LIST)

#define ALLOCATE_PLT_SYSCALLS PLT_ALLOCATE_tls(kernel_if_t, SYS_CALL_LIST)

#endif //CHERIOS_SYSCALLS_H
