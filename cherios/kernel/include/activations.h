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
#include "kernel.h"

typedef u32 aid_t;

/*
 * Kernel structure for an activation
 */
typedef	uint64_t sync_t;

#define TRANS_N(X) (X & 0xFFFF)
#define TRANS_F(X) ((X >> 16) & (0xFFFF))
#define TRANS_HD(X) (X >> 32)

#define N_TOP_BIT (1ULL << 15)
#define N_INC	   1
#define F_TOP_BIT (1ULL << 31)
#define F_INC	  (1 << 16)
#define HD_INC	  (1 << 32)

// FIXME: I made these numbers up. We should align act so they can be sufficiently large
#define MIN_OFFSET 0
#define MAX_OFFSET 0x10000
#define MAX_SEQ_NS (MAX_OFFSET - MIN_OFFSET)

// A bit too permissive but hey-hoo
#define NANO_KERNEL_USER_ACCESS_MASK ~0

// We can only store a small constant in a pointer, so we point to one of these to add an extra constant
// This requires an allocation, but only one every MAX_SEQ_NS. It is up to the user to provide reservations for new
// Reservations

typedef struct {
    struct act_t* act;
    sync_t sync_add;
} sync_indirection;

#define SI_SIZE (2 * CAP_SIZE)
#define SI_BITS (1 + CAP_SIZE_BITS)

_Static_assert(SI_SIZE >= sizeof(sync_indirection), "We need to declare a size that is larger than this struct");

typedef struct act_t
{
	/* Warning: The offset of this needs to be zero */
    CTL_t ctl;

    /* Stack for the kernel when acting on users behalf */
	capability user_kernel_stack[USER_KERNEL_STACK_SIZE / sizeof(capability)];

	/* Activation related */

	volatile uint8_t list_del_prog;
	struct act_t *volatile list_next;

	status_e status;		/* Activation status flags */

	size_t last_vaddr_fault; /* Used by exception handler to stop commit message spam */
	uint8_t commit_early_notify;

	/* Debug related */
	size_t image_base;
#if (K_DEBUG)
    uint64_t sent_n;
    uint64_t recv_n;
    uint64_t switches;
    uint64_t had_time;
    uint64_t had_time_epoch;

#ifdef HARDWARE_fpga
    // on FPGA we get some hardware counters
	STAT_DEBUG_LIST(STAT_MEMBER)
#endif

	user_stats_t user_stats;
#endif
	/* Queue related */
	queue_t * msg_queue;		/* A pointer to the message queue */
	msg_nb_t queue_mask;		/* Queue mask (cannot trust userspace
					   which has write access to queue) */
	volatile uint64_t msg_tsx;

	/* Scheduling related */
	struct spinlock_t sched_access_lock;
	enum sched_prio priority;
	sched_status_e sched_status;	/* Current status */
	uint8_t early_notify;
	uint8_t pool_id;
	uint8_t is_idle;
	register_t 	timeout_start;				/* To deal with trap around, store start + length , not end */
	register_t 	timeout_length;
	size_t 		timeout_indx;

	context_t context;	/* Space to put saved context for restore */

	/* Message pass related */
	struct sync_state {
		ret_t* sync_ret;                /* A pointer a struct to write the return message */
		volatile sync_t sync_token;		/* The sequence number we expect next */
		volatile int sync_condition;    /* A synchronisation flag for whether or not we expect a return */
        sync_indirection* current_sync_indir; /* The current indirection we are using to generate tokens */
        res_t alloc_block;              /* The user provides this to create more sync_indrections */
        size_t allocs_taken;
        size_t allocs_max;
	} sync_state;

    sync_indirection initial_indir; // Enough for MAX_SEQ_NS, then we need a new one from the user

	/* Semaphore related */
	struct act_t * semaphore_next_waiter;
#ifndef __LITE__
	char name[ACT_NAME_MAX_LEN];	/* Activation name (for debuging) */
#endif

} act_t;

_Static_assert(ACT_REQUIRED_SPACE >= sizeof(act_t) + CONTEXT_SIZE + RES_META_SIZE, "Increase the size of act required space");

#define FOR_EACH_ACT(act) for(act_t* act = act_list_start; act != NULL; act = act->list_next) { if(!act->list_del_prog)

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
extern act_t* volatile		act_list_start;
extern act_t* volatile		act_list_end; // may not be the end

extern act_t* memgt_ref;

#endif // __ASSEMBLY__

#endif
