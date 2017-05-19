/*-
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2017 Lawrence Esswood
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

#include "sys/types.h"
#include "klib.h"
#include "activations.h"
#include "queue.h"
#include "syscalls.h"
#include "ccall_trampoline.h"
#include "stddef.h"

DEFINE_ENUM_CASE(ccall_selector_t, CCALL_SELECTOR_LIST)

/*
 * Routines to handle the message queue
 */

static inline msg_nb_t safe(msg_nb_t n, msg_nb_t qmask) {
	return n & qmask;
}

static int full(queue_t * queue, msg_nb_t qmask) {
	kernel_assert(qmask > 0);
	return safe(queue->header.end+1, qmask) == queue->header.start;
}

static inline int empty(queue_t * queue) {
	return queue->header.start == queue->header.end;
}

static msg_nb_t msg_queue_fill(queue_t* queue) {
	return (queue->header.end - queue->header.start + queue->header.len) % queue->header.len;
}

int msg_push(capability c3, capability c4, capability c5,
			 register_t a0, register_t a1, register_t a2,
			 register_t v0,
			 act_t * dest, act_t * src, capability sync_token) {

	//FIXME this is still really racey, this critical section function stops interrupts, but will not work on multicore

	critical_section_enter();

	queue_t * queue = dest->msg_queue;
	msg_nb_t  qmask  = dest->queue_mask;
	kernel_assert(qmask > 0);
	int next_slot = queue->header.end;
	if(full(queue, qmask)) {
		critical_section_exit();
		return -1;
	}

	msg_t* slot = &queue->msg[next_slot];
	slot->c3 = c3;
	slot->c4 = c4;
	slot->c5 = c5;
	slot->idc = NULL;

	slot->c1 = sync_token;
	slot->c2 = (sync_token == NULL) ? NULL : kernel_seal(src, act_sync_ref_type);

	slot->a0 = a0;
	slot->a1 = a1;
	slot->a2 = a2;
	slot->v0 = v0;

	queue->header.end = safe(queue->header.end+1, qmask);

	KERNEL_TRACE("msg push", "now %lu items in %s's queue", msg_queue_fill(queue), dest->name);
	kernel_assert(!empty(queue));

	sched_receives_msg(dest);

	critical_section_exit();
	return 0;
}

int msg_queue_empty(act_t * act) {
	queue_t * queue = act->msg_queue;
	if(empty(queue)) {
		return 1;
	}
	return 0;
}

void msg_queue_init(act_t * act, queue_t * queue) {
	size_t total_length_bytes = cheri_getlen(queue);

	kernel_assert(total_length_bytes > sizeof(queue_t));

	size_t queue_len = (total_length_bytes - sizeof(queue->header)) / sizeof(msg_t);

	kernel_assert(is_power_2(queue_len));
	kernel_assert(queue_len != 0);

	act->msg_queue = queue;
	act->queue_mask = queue_len-1;

	act->msg_queue->header.start = 0;
	act->msg_queue->header.end = 0;
	act->msg_queue->header.len = queue_len;
	//todo: zero queue?
}


/* Creates a token for synchronous CCalls. This ensures the answer is unique. */
static capability get_and_set_sealed_sync_token(act_t* ccaller) {
	// FIXME No static local variables
	static sync_t unique = 0;
	unique ++;

	kernel_assert(ccaller->sync_state.sync_condition == 0);
	ccaller->sync_state.sync_token = unique;
	ccaller->sync_state.sync_condition = 1;

	capability sync_token = cheri_andperm(cheri_getdefault(), 0);
#ifdef _CHERI256_
	sync_token = cheri_setbounds(sync_token, 0);
#endif
	sync_token = cheri_setoffset(sync_token, unique);
	return kernel_seal(sync_token, act_sync_type);
}

static sync_t unseal_sync_token(capability token) {
	token = kernel_unseal(token, act_sync_type);
	return cheri_getoffset(token);
}

static int token_expected(act_t* ccaller, capability token) {
	sync_t got = unseal_sync_token(token);
	return ccaller->sync_state.sync_token == got;
}

/* This function 'returns' by setting the sync state ret values appropriately */
void act_send_message(capability c3, capability c4, capability c5,
					 register_t a0, register_t a1, register_t a2,
					 ccall_selector_t selector, register_t v0, ret_t* ret) {

	act_t* target_activation = (act_t*) get_idc();
	act_t* source_activation = kernel_curr_act;

	KERNEL_TRACE(__func__, "message from %s to %s", source_activation->name, target_activation->name);

	if(target_activation->status != status_alive) {
		KERNEL_ERROR("Trying to CCall revoked activation %s from %s",
					 target_activation->name, source_activation->name);
		ret->v0 = -1;
		ret->v1 = -1;
		ret->c3 = NULL;
		return;
	}

	// Construct a sync_token if this is a synchronous call
	capability sync_token = NULL;
	if(selector == SYNC_CALL) {
		source_activation->sync_state.sync_ret = ret;
		sync_token = get_and_set_sealed_sync_token(source_activation);
	}

	//FIXME critical section here might be a bit much?

	//TODO if we are going to switch we can (maybe) deliver this message without buffering
	msg_push(c3, c4, c5, a0, a1, a2, v0, target_activation, source_activation, sync_token);

	if(selector == SYNC_CALL) {
		//TODO in a multicore world we may spin a little if we expect the answer to be fast
		//TODO The user will indicate this with the switch flag
		sched_block(source_activation, sched_sync_block, target_activation, 0);
		KERNEL_TRACE(__func__, "%s has recieved return message from %s", source_activation->name, target_activation->name);
		return;
	} else if(selector == SEND_SWITCH) {
		sched_reschedule(target_activation, sched_runnable, 0);
	}

	ret->v0 = 0;
	ret->v1 = 0;
	ret->c3 = NULL;
	return;
}

_Static_assert(offsetof(ret_t, c3) == 0, "message return assumes these offsets");
_Static_assert(offsetof(ret_t, v0) == 32, "message return assumes these offsets");
_Static_assert(offsetof(ret_t, v1) == 40, "message return assumes these offsets");

#define MESSAGE_RETURN_RESTORE_BEFORE	\
	"daddiu $sp, $sp, -64\n"			\
	"csetoffset $c6, $c11, $sp\n"



#define MESSAGE_RETURN_RESTORE_AFTER	\
	"clc	$c3, $sp, 0($c11)\n"		\
	"cld $v0, $sp, 32($c11)\n"			\
	"cld $v1, $sp, 40($c11)\n"			\
	"daddiu $sp, $sp, 64\n"				\

int act_send_return(capability c3, capability sync_token, register_t v0, register_t v1) {

	act_t * returned_from = kernel_curr_act;
	act_t * returned_to = (act_t*) get_idc();

	if(sync_token == NULL) {
		KERNEL_TRACE(__func__, "%s did not provide a sync token", returned_from->name);
		kernel_freeze();
	}

	if(!token_expected(returned_to, sync_token)) {
		KERNEL_ERROR("wrong sequence token from creturn");
		kernel_freeze();
	}

	KERNEL_TRACE(__func__, "%s correctly makes a sync return to %s", returned_from->name, returned_to->name);

	/* At any point we might pre-empted, so the order here is important */

	/* First set the message to be picked up when the condition is unset */
	returned_to->sync_state.sync_ret->c3 = c3;
	returned_to->sync_state.sync_ret->v0 = v0;
	returned_to->sync_state.sync_ret->v1 = v1;

	/* Must no longer expect this sequence token */
	returned_to->sync_state.sync_token = 0;

	/* Set condition variable */
	returned_to->sync_state.sync_condition = 0;

	/* Make the caller runnable again */
	sched_recieve_ret(returned_to);

	sched_reschedule(returned_to, sched_runnable, 0);

	return 0;
}

void kernel_setup_trampoline() {
	/* When we create the reference to the trampoline, we also store the default capability of our creating context */
	capability* c0_store = &kernel_ccall_trampoline_c0;
	*c0_store = cheri_getdefault();
}

DEFINE_TRAMPOLINE_EXTRA(act_send_message, MESSAGE_RETURN_RESTORE_BEFORE, MESSAGE_RETURN_RESTORE_AFTER)
DEFINE_TRAMPOLINE(act_send_return)