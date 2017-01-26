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

#ifndef _OBJECT_H_
#define	_OBJECT_H_

#include "mips.h"
#include "cheric.h"

extern capability act_self_ctrl;
extern capability act_self_ref;
extern capability act_self_id;
extern capability act_self_cap;
capability	act_ctrl_get_ref(capability ctrl);
capability	act_ctrl_get_id(capability ctrl);
int	act_ctrl_revoke(capability ctrl);
int	act_ctrl_terminate(capability ctrl);
capability	act_get_cap(void);
capability	act_seal_id(capability id);

void	object_init(capability self_ctrl, capability self_cap);

void	ctor_null(void);
void	dtor_null(void);
void *	get_curr_cookie(void);
void	set_curr_cookie(void * cookie);

void * get_cookie(capability act_ref, capability act_ctrl_ref);

extern capability sync_token;
extern capability sync_caller;

extern long msg_enable;

typedef struct
{
	void * cret;
	register_t rret;
}  ret_t;

#define CCALL(selector, ...) ccall_##selector(__VA_ARGS__)
register_t ccall_1(capability act_ref, capability act_ctrl_ref, int method_nb,
		  register_t rarg1, register_t rarg2, register_t rarg3,
                  const_capability carg1, const_capability carg2, const_capability carg3);
register_t ccall_2(capability act_ref, capability act_ctrl_ref, int method_nb,
		  register_t rarg1, register_t rarg2, register_t rarg3,
                  const_capability carg1, const_capability carg2, const_capability carg3);

void	ccall_c_n(capability act_ref, capability act_ctrl_ref, int method_nb, const_capability carg);
void *	ccall_n_c(capability act_ref, capability act_ctrl_ref, int method_nb);
void *	ccall_r_c(capability act_ref, capability act_ctrl_ref, int method_nb, int rarg);
void *	ccall_c_c(capability act_ref, capability act_ctrl_ref, int method_nb, const_capability carg);
void *	ccall_rr_c(capability act_ref, capability act_ctrl_ref, int method_nb, int rarg, int rarg2);
register_t ccall_n_r(capability act_ref, capability act_ctrl_ref, int method_nb);
register_t ccall_r_r(capability act_ref, capability act_ctrl_ref, int method_nb, int rarg);
register_t ccall_c_r(capability act_ref, capability act_ctrl_ref, int method_nb, capability carg);
register_t ccall_rr_r(capability act_ref, capability act_ctrl_ref, int method_nb, int rarg, int rarg2);
register_t ccall_rc_r(capability act_ref, capability act_ctrl_ref, int method_nb, int rarg, const_capability carg);
void	ccall_cc_n(capability act_ref, capability act_ctrl_ref, int method_nb, capability carg1, capability carg2);
void	ccall_rc_n(capability act_ref, capability act_ctrl_ref, int method_nb, int rarg, capability carg);
register_t ccall_rcc_r(capability act_ref, capability act_ctrl_ref, int method_nb, register_t rarg1, capability carg1, capability carg2);
capability	ccall_rrrc_c(capability act_ref, capability act_ctrl_ref, int method_nb,
                    register_t, register_t, register_t, capability carg);
register_t ccall_rrcc_r(capability act_ref, capability act_ctrl_ref, int method_nb,
                    register_t rarg1, register_t rarg2, capability carg1, capability carg2);

#endif
