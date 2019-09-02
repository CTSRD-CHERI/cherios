/*-
 * Copyright (c) 2011 Robert N. M. Watson
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

#ifndef _CHERIOS_SCHED_H_
#define	_CHERIOS_SCHED_H_

#include "kernel.h"
#include "statcounters.h"

#if(K_DEBUG)
#if(ALL_THE_STATS)
    #define SCHED_POOL_SIZE                 1648
#else
    #define SCHED_POOL_SIZE                 1408
#endif
#else
    #define SCHED_POOL_SIZE                 1328
#endif

#define SCHED_POOL_CURRENT_ACT_OFFSET   CAP_SIZE
#define SCHED_POOL_IN_QUEUES_OFFSET     (2*CAP_SIZE)
#define SCHED_POOL_LOCK_OFFSET          ((2 *CAP_SIZE) + REG_SIZE)
#define SCHED_POOL_QUEUES_OFFSET        (3 * CAP_SIZE)

#define SCHED_QUEUE_SIZE_BITS		(4 + CAP_SIZE_BITS)
#define SCHED_QUEUE_SIZE            (CAP_SIZE * (SCHED_QUEUE_LENGTH + 1))
#define SCHED_QUEUE_END_OFFSET      (1)
#define SCHED_QUEUE_ARRAY_OFFSET	(CAP_SIZE)


#include "activations.h"

#ifndef __ASSEMBLY__

typedef struct sched_idle_init_t {
    size_t* queue_fill_pre[SMP_CORES];
} sched_idle_init_t;

void    sched_init(sched_idle_init_t*);

void    sched_schedule(uint8_t pool_id, act_t * act);
void    sched_reschedule(act_t *hint, int in_exception_handler);
void    sched_got_int(act_t* act, uint8_t cpu_id);

void	sched_create(uint8_t pool_id, act_t * act, enum sched_prio priority);
void	sched_delete(act_t * act);
void    sched_change_prio(act_t* act, enum sched_prio new_prio);

// returns how long we slept. 0 means we didn't block
register_t sched_block_until_event(act_t* act, act_t* next_hint, sched_status_e events, register_t timeout, int in_exception_handler);
void	sched_block(act_t *act, sched_status_e status);
void    sched_receive_event(act_t* act, sched_status_e events);


act_t*  sched_get_current_act_in_pool(uint8_t pool_id);
act_t*  sched_get_current_act(void);

#ifndef __LITE__
void    dump_sched(void);
#else
#define dump_sched(...)
#endif

void    sched_set_idle_act(act_t* idle_act, uint8_t pool_id);

#define SCHED_QUEUE_LENGTH 0x0f // one less than a power of 2
#define LEVEL_TO_NDX(level) ((level > PRIO_IO) ? PRIO_IO : level)


typedef struct sched_q {
    uint8_t   	act_queue_current; // index round robin. MAY NOT ACTUALLY BE CURRENT;
    uint8_t   	act_queue_end;	   // index for end;
    uint8_t		queue_ctr;		   // a counter for how many tasks at this level have been processed
    act_t* 		act_queue[SCHED_QUEUE_LENGTH];
} sched_q;


typedef struct sched_pool {
    /* The currently scheduled activation */
    act_t*		idle_act;
    act_t* 		current_act; // DONT use the current index for this. This can be accessed without a lock.
    size_t 		in_queues;
    spinlock_t 	queue_lock;
    uint8_t     pool_id;
    sched_q 	queues[SCHED_PRIO_LEVELS];
#if (K_DEBUG)
    uint32_t    last_time;
    STAT_DEBUG_LIST(STAT_MEMBER)
#endif
} sched_pool;


#define FOREACH_POOL(p) for(sched_pool* p = sched_pools; p != (sched_pools + SMP_CORES); p++)


#if (KERNEL_FASTPATH)

_Static_assert(SCHED_POOL_SIZE == sizeof(sched_pool), "Used in fastpath assembly");
_Static_assert(SCHED_POOL_CURRENT_ACT_OFFSET == offsetof(sched_pool, current_act), "Used in fastpath assembly");
_Static_assert(SCHED_POOL_LOCK_OFFSET == offsetof(sched_pool, queue_lock), "Used in fastpath assembly");
_Static_assert(SCHED_POOL_QUEUES_OFFSET == offsetof(sched_pool, queues), "Used in fastpath assembly");
_Static_assert(SCHED_POOL_IN_QUEUES_OFFSET == offsetof(sched_pool, in_queues), "Used in fastpath assembly");

// FIXME: these need static asserts
_Static_assert(SCHED_QUEUE_SIZE == sizeof(sched_q), "Used in fastpath assembly");
_Static_assert(SCHED_QUEUE_END_OFFSET == offsetof(sched_q, act_queue_end), "Used in fastpath assembly");
_Static_assert(SCHED_QUEUE_ARRAY_OFFSET == offsetof(sched_q, act_queue), "Used in fastpath assembly");

_Static_assert(SCHED_QUEUE_SIZE == (1 << SCHED_QUEUE_SIZE_BITS), "Used in fastpath assembly");

#endif


#endif

#endif /* _CHERIOS_SCHED_H_ */
