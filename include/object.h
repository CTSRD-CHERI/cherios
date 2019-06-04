/*-
 * Copyright (c) 2016 Hadrien Barral
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

#ifndef _OBJECT_H_
#define	_OBJECT_H_

#include "mips.h"
#include "cheric.h"
#include "queue.h"
#include "ccall.h"
#include "msg.h"
#include "types.h"
#include "stddef.h"
#include "nano/usernano.h"
#include "thread.h"

#define AUTO_DEDUP_ALL_FUNCTIONS    0
#define AUTO_DEDUP_STATS            0
#define AUTO_COMPACT                0

extern if_req_auth_t nanoreq_auth;

extern __thread act_control_kt act_self_ctrl;
extern __thread act_kt act_self_ref;
extern __thread act_notify_kt act_self_notify_ref;
extern __thread queue_t * act_self_queue;
extern __thread user_stats_t* own_stats;

extern int    was_secure_loaded;
extern auth_t own_auth; // like a private key for a foundation
extern found_id_t* own_found_id; // like a public key
extern startup_flags_e default_flags;
extern act_kt memmgt_ref;

act_kt try_init_memmgt_ref(void);

//TODO these should be provided by the linker/runtime

extern void __attribute__((weak)) (*msg_methods[]);
extern size_t __attribute__((weak)) msg_methods_nb;
extern void __attribute__((weak)) (*ctrl_methods[]);
extern size_t __attribute__((weak)) ctrl_methods_nb;

void    dylink_sockets(act_control_kt self_ctrl, queue_t * queue, startup_flags_e startup_flags, int first_thread);
void	object_init(act_control_kt self_ctrl, queue_t * queue,
                    kernel_if_t* kernel_if_c, tres_t cds_res,
                    startup_flags_e startup_flags, int first_thread);
void    object_destroy() __dead2;

void object_init_post_compact(startup_flags_e startup_flags, int first_thread);

void	ctor_null(void);
void	dtor_null(void);

typedef struct sync_state_t {
    act_reply_kt sync_caller;
} sync_state_t;

_Static_assert(offsetof(sync_state_t, sync_caller) == 0, "used by assembly");

extern __thread sync_state_t sync_state;

extern __thread long msg_enable;

void next_msg(void);
msg_t* get_message(void);
void pop_msg(msg_t * msg);
int msg_queue_empty(void);
// A timeout of < 0 means wait forever. Timeout will return from the routine/

#define MSG_ENTRY_TIMEOUT_ON_NOTIFY 1   // Notify is a timeout
#define MSG_ENTRY_TIMEOUT_ON_MESSAGE 2  // Messages are timeout

extern void msg_entry(int64_t timeout, int flags);
void msg_delay_return(sync_state_t* delay_store);
int msg_resume_return(capability c3, register_t  v0, register_t  v1, sync_state_t delay_store);

#if (LIGHTWEIGHT_OBJECT)
#define LW_THR __thread
#else
#define LW_THR
#endif

#endif
