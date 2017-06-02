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

extern __thread act_control_kt act_self_ctrl;
extern __thread act_kt act_self_ref;
extern __thread queue_t * act_self_queue;

//TODO these should be provided by the linker/runtime
extern void (*msg_methods[]);
extern size_t msg_methods_nb;
extern void (*ctrl_methods[]);
extern size_t ctrl_methods_nb;

void	object_init(act_control_kt self_ctrl, queue_t * queue, kernel_if_t* kernel_if_c);

void	ctor_null(void);
void	dtor_null(void);

void * get_idc_from_ref(capability act_ref, capability act_id);

typedef struct sync_state_t {
    capability sync_token;
    capability sync_caller;
} sync_state_t;

_Static_assert(offsetof(sync_state_t, sync_token) == 0, "used by assembly");
_Static_assert(offsetof(sync_state_t, sync_caller) == sizeof(capability), "used by assembly");

extern __thread sync_state_t sync_state;

extern kernel_if_t kernel_if;

extern __thread long msg_enable;

void pop_msg(msg_t * msg);
int msg_queue_empty(void);
#endif
