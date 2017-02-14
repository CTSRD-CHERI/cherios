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

#include "klib.h"
#include "queue.h"

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

int msg_push(act_t * dest, act_t * src, capability identifier, capability sync_token) {
	queue_t * queue = dest->msg_queue;
	msg_nb_t  qmask  = dest->queue_mask;
	kernel_assert(qmask > 0);
	int next_slot = queue->header.end;
	if(full(queue, qmask)) {
		return -1;
	}

	queue->msg[next_slot].a0  = src->saved_registers.mf_a0;
	queue->msg[next_slot].a1  = src->saved_registers.mf_a1;
	queue->msg[next_slot].a2  = src->saved_registers.mf_a2;

	queue->msg[next_slot].c3  = src->saved_registers.cf_c3;
	queue->msg[next_slot].c4  = src->saved_registers.cf_c4;
	queue->msg[next_slot].c5  = src->saved_registers.cf_c5;

	queue->msg[next_slot].v0  = src->saved_registers.mf_v0;
	queue->msg[next_slot].idc = identifier;
	queue->msg[next_slot].c1  = sync_token;
	queue->msg[next_slot].c2  = (sync_token == NULL) ? NULL : kernel_seal(src, act_sync_ref_type);

	queue->header.end = safe(queue->header.end+1, qmask);

	if(dest->sched_status == sched_waiting) {
		sched_receives_msg(dest);
	}
	return 0;
}

void msg_pop(act_t * act) {

	kernel_panic("Kernel should not pop");

	queue_t * queue = act->msg_queue;
	msg_nb_t  qmask  =  act->queue_mask;

	kernel_assert(!empty(queue));

	int start = queue->header.start;

	act->saved_registers.mf_a0  = queue->msg[start].a0;
	act->saved_registers.mf_a1  = queue->msg[start].a1;
	act->saved_registers.mf_a2  = queue->msg[start].a2;

	act->saved_registers.cf_c3  = queue->msg[start].c3;
	act->saved_registers.cf_c4  = queue->msg[start].c4;
	act->saved_registers.cf_c5  = queue->msg[start].c5;

	act->saved_registers.mf_v0  = queue->msg[start].v0;
	act->saved_registers.cf_idc = queue->msg[start].idc;
	act->saved_registers.cf_c1  = queue->msg[start].c1;
	act->saved_registers.cf_c2  = queue->msg[start].c2;

	queue->header.start = safe(start+1, qmask);
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
