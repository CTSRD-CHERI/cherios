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

#ifndef __ACTIVATIONS_H
#define __ACTIVATIONS_H

#include "cheric.h"
#include "queue.h"

typedef u32 aid_t;

typedef struct
{
	uint32_t expected_reply;
}  sync_t;

/*
 * Possible status for an activation
 */
typedef enum status_e
{
	status_alive = 0,
	status_revoked = 1,
	status_terminated = 2
} status_e;

/*
 * Scheduling status for an activation
 */
typedef enum sched_status_e
{
	sched_waiting,
	sched_schedulable,
	sched_runnable,
	sched_sync_block,
	sched_terminated
} sched_status_e;

/*
 * Kernel structure for an activation
 */
#define ACT_NAME_MAX_LEN (0x10)
typedef struct
{
	/* Activation related */
	aid_t aid;			/* Activation id -- redundant with array index */
	status_e status;		/* Activation status flags */
	/* Queue related */
	msg_nb_t queue_mask;		/* Queue mask (cannot trust userspace
					   which has write access to queue) */
	/* Scheduling related */
	sched_status_e sched_status;	/* Current status */
	/* CCall related */
	sync_t sync_token;		/* Helper for the synchronous CCall mecanism */
	void * act_reference;		/* Sealed reference for the activation */
	void * act_default_id;		/* Default object identifier */
	#ifndef __LITE__
	char name[ACT_NAME_MAX_LEN];	/* Activation name (for debuging) */
	#endif
} act_t;

extern reg_frame_t	kernel_exception_framep[];
extern reg_frame_t *	kernel_exception_framep_ptr;
extern act_t		kernel_acts[];
extern aid_t 		kernel_curr_act;
extern aid_t 		kernel_next_act;

#endif
