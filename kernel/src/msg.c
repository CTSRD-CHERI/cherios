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
#include "syscalls.h"
#include "ccall_trampoline.h"
#include "stddef.h"
#include "queue.h"
#include "mutex.h"

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

int msg_push(capability c3, capability c4, capability c5, capability c6,
			 register_t a0, register_t a1, register_t a2, register_t a3,
			 register_t v0,
			 act_t * dest, act_t * src, capability sync_token) {

	FAST_CRITICAL_ENTER

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
	slot->c6 = c6;

	slot->c1 = sync_token;
	slot->c2 = (sync_token == NULL) ? NULL : kernel_seal(src, act_sync_ref_type);

	slot->a0 = a0;
	slot->a1 = a1;
	slot->a2 = a2;
	slot->a3 = a3;

	slot->v0 = v0;

	queue->header.end = safe(queue->header.end+1, qmask);

	KERNEL_TRACE("msg push", "now %lu items in %s's queue", msg_queue_fill(queue), dest->name);
	kernel_assert(!empty(queue));

	sched_receives_msg(dest);

	FAST_CRITICAL_EXIT
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

    spinlock_init(&act->writer_spinlock);

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
void kernel_message_send(capability c3, capability c4, capability c5, capability c6,
					 register_t a0, register_t a1, register_t a2, register_t a3,
					 act_t* target_activation, ccall_selector_t selector, register_t v0, ret_t* ret) {

	target_activation = act_unseal_ref(target_activation);
	act_t* source_activation = (act_t*) get_idc();

	KERNEL_TRACE(__func__, "message from %s to %s", source_activation->name, target_activation->name);

	if(target_activation->status != status_alive) {
		KERNEL_ERROR("Trying to CCall revoked activation %s from %s",
					 target_activation->name, source_activation->name);
		ret->v0 = (register_t)-1;
		ret->v1 = (register_t)-1;
		ret->c3 = NULL;
		return;
	}

	// Construct a sync_token if this is a synchronous call
	capability sync_token = NULL;
	if(selector == SYNC_CALL) {
		source_activation->sync_state.sync_ret = ret;
		sync_token = get_and_set_sealed_sync_token(source_activation);
	}

    CRITICAL_LOCKED_BEGIN(&target_activation->writer_spinlock);
	msg_push(c3, c4, c5, c6, a0, a1, a2, a3, v0, target_activation, source_activation, sync_token);

	if(selector == SYNC_CALL) {
		//TODO in a multicore world we may spin a little if we expect the answer to be fast
		//TODO The user will indicate this with the switch flag
		sched_block(source_activation, sched_sync_block);
        CRITICAL_LOCKED_END(&target_activation->writer_spinlock);
        sched_reschedule(target_activation, 0);

		KERNEL_TRACE(__func__, "%s has recieved return message from %s", source_activation->name, target_activation->name);
		return;
	} else if(selector == SEND_SWITCH) {
        source_activation->sched_status = sched_runnable;
		sched_reschedule(target_activation, 0);
	}

	ret->v0 = 0;
	ret->v1 = 0;
	ret->c3 = NULL;
	return;
}

_Static_assert(offsetof(ret_t, c3) == 0, "message return assumes these offsets");
_Static_assert(offsetof(ret_t, v0) == 32, "message return assumes these offsets");
_Static_assert(offsetof(ret_t, v1) == 40, "message return assumes these offsets");

int kernel_message_reply(capability c3, register_t v0, register_t v1, act_t* caller, capability sync_token) {

	act_t * returned_from = (act_t*) get_idc();
	act_t * returned_to = kernel_unseal(caller, act_sync_ref_type);

    kernel_assert(returned_to != NULL);
    kernel_assert(returned_to->sync_state.sync_ret != NULL);

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

	sched_reschedule(returned_to, 0);

	return 0;
}