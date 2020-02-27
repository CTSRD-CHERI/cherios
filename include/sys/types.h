/*-
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

#ifndef _TYPES_H_
#define _TYPES_H_

#include "cheric.h"
#include "macroutils.h"
#include "statcounters.h"

#define DEBUG_COUNT_CALLS 0
#define STATS_COMMON_DOMAIN_OFFSET  0
#define STATS_COMPLETE_TRUST_OFFSET 8
#define STATS_TRUST_OFFSET          16
#define STATS_UNTRUSTING_OFFSET     24
#define STATS_UNTRUSTED_OFFSET      32
#define STATS_FAST_LEAFS_OFFSET     40

#ifndef __ASSEMBLY__

#if (GO_FAST)
#define EXTRA_TEMPORAL_TRACKING 0
#else
#define EXTRA_TEMPORAL_TRACKING 20
#endif

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
// TODO these values are used by fastpath assembly. Macrofy them.

typedef enum sched_status_e
{
    sched_runnable = 0,
    sched_running = 1,
    sched_terminated = 2,
    sched_waiting       = 0x10,      /* Waiting on a message */
    sched_sync_block    = 0x20,      /* Waiting on a return  */
    sched_sem           = 0x40,      /* Waiting on a kernel semaphore */
    sched_wait_notify   = 0x80,      /* Waiting on a notify */
    sched_wait_timeout  = 0x100,     /* Waiting on a timeout */
    sched_wait_commit   = 0x200,     /* Waiting on a vmem commit */
    sched_wait_fastpath = 0x400,     /* Waiting on a message, fast path supported. WARN: Used by fastpath assembly. */
} sched_status_e;

/* Scheduling priorities. Higher gets exponentially more time. */
#define SCHED_PRIO_FACTOR 0x10 // Each level of priority gives this factor more time. Must be power 2.
#define SCHED_PRIO_LEVELS 5
enum sched_prio {
    PRIO_IDLE = 0, // only do this when there are no activations at higher levels
    PRIO_LOW = 1,
    PRIO_MID = 2,
    PRIO_HIGH = 3,
    PRIO_IO = 4, // Temporay, can be toggled on off alongside another priority (keep power 2)
};

typedef struct
{
    capability c3;
    register_t v0;
    register_t v1;
}  ret_t;

typedef struct cap_pair {
    capability code;
    capability data;
} cap_pair;

#ifdef HARDWARE_fpga

#if(ALL_THE_STATS)

    #define ITM(Name, X, Y, ITEM, ...) ITEM(Name, STRINGIFY(Name), __VA_ARGS__)
    #define STAT_DEBUG_LIST(ITEM, ...) STAT_ALL_LIST(ITM, ITEM, __VA_ARGS__)

#else

#define STAT_DEBUG_LIST(ITEM,...) 		\
	ITEM(cycle, 	" cycle ", __VA_ARGS__)			\
	ITEM(inst, 		" insts ",  __VA_ARGS__)				\
	ITEM(dtlb_miss, "dtlbmis", __VA_ARGS__)		\
	ITEM(itlb_miss, "itlbmis", __VA_ARGS__)	\
	ITEM(dcache_read_hit, "drd hit", __VA_ARGS__) \
	ITEM(dcache_read_miss, "drdmiss", __VA_ARGS) \
	ITEM(dcache_write_hit, "dwr hit", __VA_ARGS__)\
	ITEM(dcache_write_miss, "dwrmiss", __VA_ARGS__)

#endif
#else
#define STAT_DEBUG_LIST(ITEM, ...)
#endif

// WARN: I have not put any static asserts for these offsets. Change with care
#if (DEBUG_COUNT_CALLS)
#define USER_STATS_CALLS(ITEM, ...) \
    ITEM(common_domain, "co-dom", __VA_ARGS__)\
    ITEM(complete_trusting, "ctrust", __VA_ARGS__)\
    ITEM(trusting, "strust", __VA_ARGS__)\
    ITEM(untrusting, "utrstg", __VA_ARGS__)\
    ITEM(untrusted, "utrstd", __VA_ARGS__)\
    ITEM(fast_leaf, "fstlef", __VA_ARGS__)
#else
#define USER_STATS_CALLS(...)
#endif

#define USER_STATS_LIST(ITEM, ...) \
    USER_STATS_CALLS(ITEM,__VA_ARGS__)\
    ITEM(temporal_depth, "tdepth", __VA_ARGS__)\
    ITEM(temporal_reqs, "treqst", __VA_ARGS__)

#define STAT_MEMBER(name, ...) uint64_t name;

typedef struct user_stats_s {
    USER_STATS_LIST(STAT_MEMBER)
    uint32_t stacks_at_level[EXTRA_TEMPORAL_TRACKING];

} user_stats_t;

typedef struct act_info_s {
    char* name;
    uint64_t had_time;
    uint64_t switches;
    uint64_t sent_n;
    uint64_t received_n;
    uint64_t commit_faults;
    uint16_t queue_fill;
    status_e status;
    sched_status_e sched_status;
    uint8_t cpu;

    STAT_DEBUG_LIST(STAT_MEMBER)
    user_stats_t user_stats;

    uint64_t had_time_epoch;
} act_info_t;

#define ACT_REQUIRED_SPACE ((8 * 1024) - (RES_META_SIZE * 2))

typedef capability act_kt;
typedef capability act_control_kt;
typedef capability act_reply_kt;
typedef capability act_notify_kt;

#endif

#endif
