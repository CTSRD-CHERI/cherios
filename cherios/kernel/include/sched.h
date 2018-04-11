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

#include "activations.h"

typedef struct sched_idle_init_t {
    size_t* queue_fill_pre[SMP_CORES];
} sched_idle_init_t;

void    sched_init(sched_idle_init_t*);

void    sched_schedule(uint8_t pool_id, act_t * act);
void    sched_reschedule(act_t *hint, int in_exception_handler);

void	sched_create(uint8_t pool_id, act_t * act);
void	sched_delete(act_t * act);

void    sched_block_until_event(act_t* act, act_t* next_hint, sched_status_e events);

void    sched_block_until_ret(act_t * act, act_t * next_hint);
void    sched_block_until_msg(act_t * act, act_t * next_hint);
void sched_wait_for_notify(act_t* act, act_t* next_hint, int notify_on_message);


void	sched_block(act_t *act, sched_status_e status);
void    sched_receives_sem_signal(act_t * act);
void	sched_receives_msg(act_t * act);
void    sched_recieve_ret(act_t * act);

void sched_receives_notify(act_t * act);


size_t* sched_get_queue_fill_pointer(uint8_t pool_id);
act_t*  sched_get_current_act_in_pool(uint8_t pool_id);
act_t*  sched_get_current_act(void);
void    sched_set_idle_act(act_t* idle_act, uint8_t pool_id);
#endif /* _CHERIOS_SCHED_H_ */
