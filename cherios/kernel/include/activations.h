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

#ifndef __ACTIVATIONS_H
#define __ACTIVATIONS_H

#define ACT_NAME_MAX_LEN (0x10)
#define USER_KERNEL_STACK_SIZE 4096

#ifndef __ASSEMBLY__

#include "cheric.h"
#include "types.h"
#include "stddef.h"
#include "nanokernel.h"
#include "queue.h"
#include "mutex.h"

typedef u32 aid_t;

/*
 * Kernel structure for an activation
 */
typedef	uint64_t sync_t;

typedef struct act_t
{
	/* Stack for the kernel when acting on users behalf */
	/* Warning: The offset of this is assumed by assembly */
	capability user_kernel_stack[USER_KERNEL_STACK_SIZE / sizeof(capability)];

	register_t stack_guard;
	/* Activation related */
	status_e status;		/* Activation status flags */

	/* Debug related */
	size_t image_base;

	/* Queue related */
	queue_t * msg_queue;		/* A pointer to the message queue */
    struct spinlock_t writer_spinlock;
	msg_nb_t queue_mask;		/* Queue mask (cannot trust userspace
					   which has write access to queue) */
	/* Used by the scheduler to beat races without having to turn off premption */
	int message_recieve_flag;
	/* Scheduling related */
	sched_status_e sched_status;	/* Current status */

	context_t context;	/* Space to put saved context for restore */

	/* CCall related */
	struct sync_state {
		ret_t* sync_ret;
		sync_t sync_token;		/* Helper for the synchronous CCall mecanism */
		int sync_condition;
	} sync_state;

	/* Semaphore related */
	struct act_t * semaphore_next_waiter;
#ifndef __LITE__
	char name[ACT_NAME_MAX_LEN];	/* Activation name (for debuging) */
#endif

} act_t;

/* Assumed by assembly */
_Static_assert(offsetof(act_t, user_kernel_stack) == 0,
			   "Kernel ccall trampolines assume the stack is the first member");
_Static_assert(offsetof(act_t, stack_guard) == USER_KERNEL_STACK_SIZE,
			   "Kernel ccall trampolines assume the guard is the second member");

/* Control references are just references with a different type */
typedef act_t act_control_t;

//FIXME scrap these, the kernel should not allocate memory.
/* global array of all activations */
extern act_t		kernel_acts[];
/* The index of the next activation to put in the above array. NOT to do with scheduling.*/
extern aid_t 		kernel_next_act;

/* The currently scheduled activation */
extern act_t* 		kernel_curr_act;

extern act_t* memgt_ref;

#endif // __ASSEMBLY__

#endif
