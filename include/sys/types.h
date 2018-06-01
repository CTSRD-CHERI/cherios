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
    sched_runnable = 0,
    sched_running = 1,
    sched_terminated = 2,
    sched_waiting       = 0x10,      /* Waiting on a message */
    sched_sync_block    = 0x20,      /* Waiting on a return  */
    sched_sem           = 0x40,      /* Waiting on a kernel semaphore */
    sched_wait_notify   = 0x80,      /* Waiting on a notify */
    sched_wait_timeout  = 0x100,     /* Waiting on a timeout */
    sched_wait_commit   = 0x200,     /* Waiting on a vmem commit */
} sched_status_e;

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

#define ACT_REQUIRED_SPACE ((8 * 1024) - (RES_META_SIZE * 2))

typedef capability act_kt;
typedef capability act_control_kt;
typedef capability act_reply_kt;
typedef capability act_notify_kt;

typedef capability sync_token_t;
#endif
