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

#include "mips.h"
#include "object.h"
#include "cheric.h"
#include "assert.h"
#include "namespace.h"
#include "queue.h"
#include "syscalls.h"

capability act_self_ctrl = NULL;
capability act_self_ref  = NULL;
capability act_self_id   = NULL;
capability act_self_cap   = NULL;
queue_t * act_self_queue = NULL;

void object_init(capability self_ctrl, capability self_cap, queue_t * queue) {
	act_self_ctrl = self_ctrl;
	act_self_ref  = act_ctrl_get_ref(self_ctrl);
	act_self_id   = act_ctrl_get_id(self_ctrl);
	act_self_queue = queue;
	act_self_cap = self_cap;
}

capability act_get_cap(void) {
	return act_self_cap;
}

capability act_ctrl_get_ref(capability ctrl) {
	capability ref;
	SYSCALL_c3_retc(ACT_CTRL_GET_REF, ctrl, ref);
	return ref;
}

capability act_ctrl_get_id(capability ctrl) {
	capability ref;
	SYSCALL_c3_retc(ACT_CTRL_GET_ID, ctrl, ref);
	return ref;
}

int act_ctrl_revoke(capability ctrl) {
	int ret;
	SYSCALL_c3_retr(ACT_REVOKE, ctrl, ret);
	return ret;
}

int act_ctrl_terminate(capability ctrl) {
	int ret;
	SYSCALL_c3_retr(ACT_TERMINATE, ctrl, ret);
	return ret;
}

capability act_seal_id(capability id) {
	capability sid;
	SYSCALL_c3_retc(ACT_SEAL_IDENTIFIER, id, sid);
	return sid;
}

void ctor_null(void) {
	return;
}

void dtor_null(void) {
	return;
}

void * get_idc(void) {
	void * object;
	__asm__ (
		"cmove %[object], $idc \n"
		: [object]"=C" (object));
	return object;
}

void set_idc(void *cookie) {
	__asm__ (
		"cmove $idc, %[cookie] \n"
		:: [cookie]"C" (cookie));
}

void * get_idc_from_ref(capability act_ref, capability act_id
) {
	return ccall_n_c(act_ref, act_id, -1);
}

/*
 * CCall helpers
 */

#define CCALL_ASM_CSCB "cmove $c1, %[act_ref] \n" "cmove $c2, %[act_id] \n" "move $v0, %[method_nb] \n"
#define CCALL_INSTR "ccall $c1, $c2, %[n]\n"
#define CCALL_INOPS(ccalln) [act_ref]"C" (act_ref), [act_id]"C" (act_id), [method_nb]"r" (method_nb), [n]"I" (ccalln)
#define CCALL_CLOBS "$c1","$c2","$c3","$c4","$c5","v0","v1","a0","a1","a2"
#define CCALL_TOP \
	assert(VCAPS(act_ref, 0, VCAP_X)); \
	assert(VCAPS(act_id, 0, VCAP_W)); \
	ret_t ret; \
	__asm__ __volatile__ ( \
		CCALL_ASM_CSCB \
		"move  $a0, %[rarg1] \n" \
		"move  $a1, %[rarg2] \n" \
		"move  $a2, %[rarg3] \n" \
		"cmove $c3, %[carg1] \n" \
		"cmove $c4, %[carg2] \n" \
		"cmove $c5, %[carg3] \n" \

#define CCALL_BOTTOM(ccalln) \
		"move  %[rret], $v0  \n" \
		"cmove %[cret], $c3  \n" \
		: [rret]"=r" (ret.rret), [cret]"=C" (ret.cret) \
		: CCALL_INOPS(ccalln), [rarg1]"r" (rarg1), [rarg2]"r" (rarg2), [rarg3]"r" (rarg3), \
		               [carg1]"C" (carg1), [carg2]"C" (carg2), [carg3]"C" (carg3) \
		: CCALL_CLOBS);

#define CCALLS(...) CCALL(SYNC_CALL, __VA_ARGS__)

register_t ccall_SEND(capability act_ref, capability act_id, int method_nb,
					  register_t rarg1, register_t rarg2, register_t rarg3,
					  const_capability carg1, const_capability carg2, const_capability carg3) {
	CCALL_TOP
	CCALL_INSTR
	CCALL_BOTTOM(SEND)
	return ret.rret;
}

register_t ccall_SEND_SWITCH(capability act_ref, capability act_id, int method_nb,
							 register_t rarg1, register_t rarg2, register_t rarg3,
							 const_capability carg1, const_capability carg2, const_capability carg3) {
	CCALL_TOP
	CCALL_INSTR
	CCALL_BOTTOM(SEND_SWITCH)
	return ret.rret;
}

inline ret_t ccall_SYNC_CALL(capability act_ref, capability act_id, int method_nb,
							 register_t rarg1, register_t rarg2, register_t rarg3,
							 const_capability carg1, const_capability carg2, const_capability carg3) {
	CCALL_TOP
	CCALL_INSTR
	CCALL_BOTTOM(SYNC_CALL)
	return ret;
}
ret_t ccall_SYNC_CALL(capability act_ref, capability act_id, int method_nb,
					  register_t rarg1, register_t rarg2, register_t rarg3,
					  const_capability carg1, const_capability carg2, const_capability carg3);


void ccall_c_n(capability act_ref, capability act_id
		, int method_nb, const_capability carg) {
	CCALLS(act_ref, act_id, method_nb, 0, 0, 0, carg, NULL, NULL);
}

capability ccall_n_c(capability act_ref, capability act_id
		, int method_nb) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, 0, 0, 0, NULL, NULL, NULL);
	return ret.cret;
}

capability ccall_r_c(capability act_ref, capability act_id
		, int method_nb, int rarg) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, rarg, 0, 0, NULL, NULL, NULL);
	return ret.cret;
}

capability ccall_c_c(capability act_ref, capability act_id
		, int method_nb, const_capability carg) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, 0, 0, 0, carg, NULL, NULL);
	return ret.cret;
}


capability ccall_rr_c(capability act_ref, capability act_id
		, int method_nb, int rarg1, int rarg2) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, rarg1, rarg2, 0, NULL, NULL, NULL);
	return ret.cret;
}

register_t ccall_n_r(capability act_ref, capability act_id
		, int method_nb) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, 0, 0, 0, NULL, NULL, NULL);
	return ret.rret;
}

register_t ccall_r_r(capability act_ref, capability act_id
		, int method_nb, int rarg) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, rarg, 0, 0, NULL, NULL, NULL);
	return ret.rret;
}

register_t ccall_c_r(capability act_ref, capability act_id
		, int method_nb, capability carg) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, 0, 0, 0, carg, NULL, NULL);
	return ret.rret;
}

register_t ccall_rr_r(capability act_ref, capability act_id
		, int method_nb, int rarg1, int rarg2) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, rarg1, rarg2, 0, NULL, NULL, NULL);
	return ret.rret;
}

register_t ccall_rc_r(capability act_ref, capability act_id
		, int method_nb, int rarg, const_capability carg) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, rarg, 0, 0, carg, NULL, NULL);
	return ret.rret;
}

void ccall_rc_n(capability act_ref, capability act_id
		, int method_nb, int rarg, capability carg) {
	CCALLS(act_ref, act_id, method_nb, rarg, 0, 0, carg, NULL, NULL);
}

void ccall_cc_n(capability act_ref, capability act_id
		, int method_nb, capability carg1, capability carg2) {
	CCALLS(act_ref, act_id, method_nb, 0, 0, 0, carg1, carg2, NULL);
}

register_t ccall_rcc_r(capability act_ref, capability act_id
		, int method_nb, register_t rarg, capability carg1, capability carg2) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, rarg, 0, 0, carg1, carg2, NULL);
	return ret.rret;
}

capability ccall_rrrc_c(capability act_ref, capability act_id
		, int method_nb,
                    register_t rarg1, register_t rarg2, register_t rarg3, capability carg) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, rarg1, rarg2, rarg3, carg, NULL, NULL);
	return ret.cret;
}

register_t ccall_rrcc_r(capability act_ref, capability act_id
		, int method_nb,
                    register_t rarg1, register_t rarg2, capability carg1, capability carg2) {
	ret_t ret = CCALLS(act_ref, act_id, method_nb, rarg1, rarg2, 0, carg1, carg2, NULL);
	return ret.rret;
}
