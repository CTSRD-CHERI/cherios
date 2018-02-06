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
#include "nano/nanokernel.h"
#include "queue.h"
#include "mutex.h"
#include "dylink.h"

typedef u32 aid_t;

/*
 * Kernel structure for an activation
 */
typedef	uint64_t sync_t;

typedef struct act_t
{
	/* Warning: The offset of this needs to be zero */
    CTL_t ctl;

    /* Stack for the kernel when acting on users behalf */
	capability user_kernel_stack[USER_KERNEL_STACK_SIZE / sizeof(capability)];

	/* Activation related */

	struct act_t* list_next;
	struct act_t* list_prev;

	status_e status;		/* Activation status flags */

	size_t last_vaddr_fault; /* Used by exception handler to stop commit message spam */

	/* Debug related */
	size_t image_base;

	/* Queue related */
	queue_t * msg_queue;		/* A pointer to the message queue */
    struct spinlock_t writer_spinlock;
	msg_nb_t queue_mask;		/* Queue mask (cannot trust userspace
					   which has write access to queue) */


	/* Scheduling related */
	struct spinlock_t sched_access_lock;
	sched_status_e sched_status;	/* Current status */
	uint8_t pool_id;

	context_t context;	/* Space to put saved context for restore */

	/* CCall related */
	struct sync_state {
		ret_t* sync_ret;
		sync_t sync_token;		/* Helper for the synchronous CCall mecanism */
		volatile int sync_condition;
	} sync_state;

	/* Semaphore related */
	struct act_t * semaphore_next_waiter;
#ifndef __LITE__
	char name[ACT_NAME_MAX_LEN];	/* Activation name (for debuging) */
#endif

} act_t;

_Static_assert(ACT_REQUIRED_SPACE >= sizeof(act_t), "Increase the size of act required space");

#define FOR_EACH_ACT(act) for(act_t* act = act_list_start; act != NULL; act = act->list_next)

/* Assumed by assembly */
_Static_assert(offsetof(act_t, ctl) == 0,
			   "Dynamic linking requires a ctl struct to be the first item");

/* Control references are just references with a different type */
typedef act_t act_control_t;

/* global array of all STATICICALLY ALLOCATED activations. Use the linked list to find them all */
extern act_t		kernel_acts[];
/* The index of the next activation to put in the above array. NOT to do with scheduling.*/
extern aid_t 		kernel_next_act;

extern struct spinlock_t 	act_list_lock;
extern act_t*		act_list_start;
extern act_t*		act_list_end;

extern act_t* memgt_ref;

#endif // __ASSEMBLY__

#endif
