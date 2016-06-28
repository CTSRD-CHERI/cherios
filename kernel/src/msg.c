/*-
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

/*
 * Routines to handle the message queue
 */

static msg_nb_t safe(msg_nb_t n, msg_nb_t qlen) {
	assert(qlen > 0);
	return n%(qlen);
}

static int full(queue_t * queue, msg_nb_t qlen) {
	assert(qlen > 0);
	return safe(queue->end+1, qlen) == queue->start;
}

static int empty(queue_t * queue) {
	return queue->start == queue->end;
}

int msg_push(int dest, int src, void * identifier, void * sync_token) {
	queue_t * queue = &kernel_acts[dest].queue;
	msg_nb_t  qlen  =  kernel_acts[dest].queue_len;
	assert(qlen > 0);
	int next_slot = queue->end;
	if(full(queue, qlen)) {
		kernel_panic("queue full");
		return -1;
	}

	queue->msg[next_slot].a0  = kernel_exception_framep[src].mf_a0;
	queue->msg[next_slot].a1  = kernel_exception_framep[src].mf_a1;
	queue->msg[next_slot].a2  = kernel_exception_framep[src].mf_a2;

	queue->msg[next_slot].c3  = kernel_exception_framep[src].cf_c3;
	queue->msg[next_slot].c4  = kernel_exception_framep[src].cf_c4;
	queue->msg[next_slot].c5  = kernel_exception_framep[src].cf_c5;

	queue->msg[next_slot].v0  = kernel_exception_framep[src].mf_v0;
	queue->msg[next_slot].idc = identifier;
	queue->msg[next_slot].c1  = sync_token;

	queue->end = safe(queue->end+1, qlen);

	if(kernel_acts[dest].status == status_waiting) {
		kernel_acts[dest].status = status_schedulable;
	}
	return 0;
}

void msg_pop(int act) {
	queue_t * queue = &kernel_acts[act].queue;
	msg_nb_t  qlen  =  kernel_acts[act].queue_len;

	assert(kernel_acts[act].status == status_schedulable);
	assert(!empty(queue));

	int start = queue->start;

	kernel_exception_framep[act].mf_a0  = queue->msg[start].a0;
	kernel_exception_framep[act].mf_a1  = queue->msg[start].a1;
	kernel_exception_framep[act].mf_a2  = queue->msg[start].a2;

	kernel_exception_framep[act].cf_c3  = queue->msg[start].c3;
	kernel_exception_framep[act].cf_c4  = queue->msg[start].c4;
	kernel_exception_framep[act].cf_c5  = queue->msg[start].c5;

	kernel_exception_framep[act].mf_v0  = queue->msg[start].v0;
	kernel_exception_framep[act].cf_idc = queue->msg[start].idc;
	kernel_exception_framep[act].cf_c1  = queue->msg[start].c1;

	queue->start = safe(start+1, qlen);
}

status_e msg_try_wait(int act) {
	queue_t * queue = &kernel_acts[act].queue;
	if(empty(queue)) {
		return status_waiting;
	} else {
		return status_schedulable;
	}
}
