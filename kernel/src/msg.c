/*-
 * Copyright (c) 2016 Hongyan Xia
 * Copyright (c) 2016 Hadrien Barral
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

static queue_t msg_queues[MAX_ACTIVATIONS];

/*
 * Routines to handle the message queue
 */

static inline msg_nb_t safe(msg_nb_t n, msg_nb_t qmask) {
	return n & qmask;
}

static int full(queue_t * queue, msg_nb_t qmask) {
	kernel_assert(qmask > 0);
	return safe(queue->end+1, qmask) == queue->start;
}

static inline int empty(queue_t * queue) {
	return queue->start == queue->end;
}

int msg_push(int dest, int src, void * identifier, uint64_t sync_token) {
	queue_t * queue = msg_queues + dest;
	msg_nb_t  qmask  = kernel_acts[dest].queue_mask;
	kernel_assert(qmask > 0);
	int next_slot = queue->end;
	if(full(queue, qmask)) {
		return -1;
	}

	queue->msg[next_slot].a0  = kernel_exception_framep[src].mf_a0;
	queue->msg[next_slot].a1  = kernel_exception_framep[src].mf_a1;
	queue->msg[next_slot].a2  = kernel_exception_framep[src].mf_a2;
	queue->msg[next_slot].a3  = kernel_exception_framep[src].mf_a3;

	queue->msg[next_slot].v0  = kernel_exception_framep[src].mf_v0;
	queue->msg[next_slot].v1  = kernel_exception_framep[src].mf_v1;
	queue->msg[next_slot].t0  = sync_token;

	queue->end = safe(queue->end+1, qmask);

	if(kernel_acts[dest].sched_status == sched_waiting) {
		sched_d2a(dest, sched_schedulable);
	}
    return 0;
}

void msg_pop(aid_t act) {
	queue_t * queue = msg_queues + act;
	msg_nb_t  qmask  =  kernel_acts[act].queue_mask;

	kernel_assert(kernel_acts[act].sched_status == sched_schedulable);
	kernel_assert(!empty(queue));

	int start = queue->start;

	kernel_exception_framep[act].mf_a0  = queue->msg[start].a0;
	kernel_exception_framep[act].mf_a1  = queue->msg[start].a1;
	kernel_exception_framep[act].mf_a2  = queue->msg[start].a2;
	kernel_exception_framep[act].mf_a3  = queue->msg[start].a3;

	kernel_exception_framep[act].mf_v0  = queue->msg[start].v0;
	kernel_exception_framep[act].mf_v1  = queue->msg[start].v1;
	kernel_exception_framep[act].mf_t0  = queue->msg[start].t0;

	queue->start = safe(start+1, qmask);
}

int msg_queue_empty(aid_t act) {
	queue_t * queue = msg_queues + act;
	if(empty(queue)) {
		return 1;
	}
	return 0;
}

void msg_queue_init(aid_t act) {
	msg_queues[act].start = 0;
	msg_queues[act].end = 0;
	msg_queues[act].len = MAX_MSG;
	//todo: zero queue?
}
