/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "mips.h"
#include "object.h"
#include "cheric.h"
#include "assert.h"
#include "namespace.h"
#include "queue.h"
#include "syscalls.h"

capability act_self_ctrl = NULL;
capability act_self_ref  = NULL;
capability act_self_cap   = NULL;
queue_t * act_self_queue = NULL;
kernel_if_t kernel_if;

void object_init(capability self_ctrl, capability self_cap, queue_t * queue) {
	act_self_ctrl = self_ctrl;
	act_self_ref  = act_ctrl_get_ref(self_ctrl);
	act_self_queue = queue;
	act_self_cap = self_cap;
}

capability act_get_cap(void) {
	return act_self_cap;
}

capability act_ctrl_get_ref(capability ctrl) {
	capability ref;
	SYSCALL_c3_retc(ACT_CTRL_GET_REF, ctrl, ref);
	return ref;
}

int act_ctrl_revoke(capability ctrl) {
	int ret;
	SYSCALL_c3_retr(ACT_REVOKE, ctrl, ret);
	return ret;
}

int act_ctrl_terminate(capability ctrl) {
	int ret;
	SYSCALL_c3_retr(ACT_TERMINATE, ctrl, ret);
	return ret;
}

capability act_seal_id(capability id) {
	assert(0 && "TODO");
	return NULL;
}

void ctor_null(void) {
	return;
}

void dtor_null(void) {
	return;
}