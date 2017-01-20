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

#include "mips.h"
#include "object.h"
#include "cheric.h"
#include "assert.h"
#include "namespace.h"

void * act_self_ctrl = NULL;
void * act_self_ref  = NULL;
void * act_self_id   = NULL;
void * act_self_cap  = NULL;

void object_init(void * self_ctrl, void* self_cap) {
	assert(self_ctrl != NULL);

	act_self_ctrl = self_ctrl;
	act_self_ref  = act_ctrl_get_ref(self_ctrl);
	act_self_id   = act_ctrl_get_id(self_ctrl);
	act_self_cap  = self_cap;
}

void * act_get_ctrl(void) {
	return act_self_ctrl;
}

void * act_get_cap(void) {
	return act_self_cap;
}

void * act_ctrl_get_ref(void * ctrl) {
	void * ref;
	__asm__ (
		"li    $v0, 21      \n"
		"cmove $c3, %[ctrl] \n"
		"syscall            \n"
		"cmove %[ref], $c3  \n"
		: [ref]"=C" (ref)
		: [ctrl]"C" (ctrl)
		: "v0", "$c3");
	return ref;
}

void * act_ctrl_get_id(void * ctrl) {
	void * ref;
	__asm__ (
		"li    $v0, 22      \n"
		"cmove $c3, %[ctrl] \n"
		"syscall            \n"
		"cmove %[ref], $c3  \n"
		: [ref]"=C" (ref)
		: [ctrl]"C" (ctrl)
		: "v0", "$c3");
	return ref;
}

int act_ctrl_revoke(void * ctrl) {
	int ret;
	__asm__ (
		"li    $v0, 25      \n"
		"cmove $c3, %[ctrl] \n"
		"syscall            \n"
		"move %[ret], $v0   \n"
		: [ret]"=r" (ret)
		: [ctrl]"C" (ctrl)
		: "v0", "$c3");
	return ret;
}

int act_ctrl_terminate(void * ctrl) {
	int ret;
	__asm__ (
		"li    $v0, 26      \n"
		"cmove $c3, %[ctrl] \n"
		"syscall            \n"
		"move %[ret], $v0   \n"
		: [ret]"=r" (ret)
		: [ctrl]"C" (ctrl)
		: "v0", "$c3");
	return ret;
}

void * act_seal_id(void * id) {
	void * sid;
	__asm__ (
		"li    $v0, 29      \n"
		"cmove $c3, %[id]   \n"
		"syscall            \n"
		"cmove %[sid], $c3  \n"
		: [sid]"=C" (sid)
		: [id]"C" (id)
		: "v0", "$c3");
	return sid;
}

void ctor_null(void) {
	return;
}

void dtor_null(void) {
	return;
}

void * get_curr_cookie(void) {
	void * object;
	__asm__ (
		"cmove %[object], $c26 \n"
		: [object]"=C" (object));
	return object;
}

void set_curr_cookie(void * cookie) {
	__asm__ (
		"cmove $c26, %[cookie] \n"
		:: [cookie]"C" (cookie));
}

void * get_cookie(void * cb, void * cs) {
	return ccall_n_c(cb, cs, -1);
}

/*
 * CCall helpers
 */

#define CCALL_ASM_CSCB "cmove $c1, %[cb] \n" "cmove $c2, %[cs] \n" "move $v0, %[method_nb] \n"
#define CCALL_INSTR(n) "ccall $c1, $c2, " #n "\n"
#define CCALL_INOPS [cb]"C" (cb), [cs]"C" (cs), [method_nb]"r" (method_nb)
#define CCALL_CLOBS "$c1","$c2","$c3","$c4","$c5","v0","v1","a0","a1","a2"
#define CCALL_TOP \
	assert(VCAPS(cb, 0, VCAP_X)); \
	assert(VCAPS(cs, 0, VCAP_W)); \
	ret_t ret; \
	__asm__ __volatile__ ( \
		CCALL_ASM_CSCB \
		"move  $a0, %[rarg1] \n" \
		"move  $a1, %[rarg2] \n" \
		"move  $a2, %[rarg3] \n" \
		"cmove $c3, %[carg1] \n" \
		"cmove $c4, %[carg2] \n" \
		"cmove $c5, %[carg3] \n" \

#define CCALL_BOTTOM \
		"move  %[rret], $v0  \n" \
		"cmove %[cret], $c3  \n" \
		: [rret]"=r" (ret.rret), [cret]"=C" (ret.cret) \
		: CCALL_INOPS, [rarg1]"r" (rarg1), [rarg2]"r" (rarg2), [rarg3]"r" (rarg3), \
		               [carg1]"C" (carg1), [carg2]"C" (carg2), [carg3]"C" (carg3) \
		: CCALL_CLOBS);

#define CCALLS(...) CCALL(4, __VA_ARGS__)

register_t ccall_1(void * cb, void * cs, int method_nb,
		  register_t rarg1, register_t rarg2, register_t rarg3,
                  const void * carg1, const void * carg2, const void * carg3) {
	CCALL_TOP
		CCALL_INSTR(1)
	CCALL_BOTTOM
	return ret.rret;
}

register_t ccall_2(void * cb, void * cs, int method_nb,
		  register_t rarg1, register_t rarg2, register_t rarg3,
                  const void * carg1, const void * carg2, const void * carg3) {
	CCALL_TOP
		CCALL_INSTR(2)
	CCALL_BOTTOM
	return ret.rret;
}

inline ret_t ccall_4(void * cb, void * cs, int method_nb,
		  register_t rarg1, register_t rarg2, register_t rarg3,
                  const void * carg1, const void * carg2, const void * carg3) {
	CCALL_TOP
		CCALL_INSTR(4)
	CCALL_BOTTOM
	return ret;
}


void ccall_c_n(void * cb, void * cs, int method_nb, const void * carg) {
	CCALLS(cb, cs, method_nb, 0, 0, 0, carg, NULL, NULL);
}

void * ccall_n_c(void * cb, void * cs, int method_nb) {
	ret_t ret = CCALLS(cb, cs, method_nb, 0, 0, 0, NULL, NULL, NULL);
	return ret.cret;
}

void * ccall_r_c(void * cb, void * cs, int method_nb, int rarg) {
	ret_t ret = CCALLS(cb, cs, method_nb, rarg, 0, 0, NULL, NULL, NULL);
	return ret.cret;
}

void * ccall_c_c(void * cb, void * cs, int method_nb, const void * carg) {
	ret_t ret = CCALLS(cb, cs, method_nb, 0, 0, 0, carg, NULL, NULL);
	return ret.cret;
}


void * ccall_rr_c(void * cb, void * cs, int method_nb, int rarg1, int rarg2) {
	ret_t ret = CCALLS(cb, cs, method_nb, rarg1, rarg2, 0, NULL, NULL, NULL);
	return ret.cret;
}

register_t ccall_n_r(void * cb, void * cs, int method_nb) {
	ret_t ret = CCALLS(cb, cs, method_nb, 0, 0, 0, NULL, NULL, NULL);
	return ret.rret;
}

register_t ccall_r_r(void * cb, void * cs, int method_nb, int rarg) {
	ret_t ret = CCALLS(cb, cs, method_nb, rarg, 0, 0, NULL, NULL, NULL);
	return ret.rret;
}

register_t ccall_c_r(void * cb, void * cs, int method_nb, void * carg) {
	ret_t ret = CCALLS(cb, cs, method_nb, 0, 0, 0, carg, NULL, NULL);
	return ret.rret;
}

register_t ccall_rr_r(void * cb, void * cs, int method_nb, int rarg1, int rarg2) {
	ret_t ret = CCALLS(cb, cs, method_nb, rarg1, rarg2, 0, NULL, NULL, NULL);
	return ret.rret;
}

register_t ccall_rc_r(void * cb, void * cs, int method_nb, int rarg, const void * carg) {
	ret_t ret = CCALLS(cb, cs, method_nb, rarg, 0, 0, carg, NULL, NULL);
	return ret.rret;
}

void ccall_rc_n(void * cb, void * cs, int method_nb, int rarg, void * carg) {
	CCALLS(cb, cs, method_nb, rarg, 0, 0, carg, NULL, NULL);
}

void ccall_cc_n(void * cb, void * cs, int method_nb, void * carg1, void * carg2) {
	CCALLS(cb, cs, method_nb, 0, 0, 0, carg1, carg2, NULL);
}

register_t ccall_rcc_r(void * cb, void * cs, int method_nb, register_t rarg, void * carg1, void * carg2) {
	ret_t ret = CCALLS(cb, cs, method_nb, rarg, 0, 0, carg1, carg2, NULL);
	return ret.rret;
}

void * ccall_rrrc_c(void * cb, void * cs, int method_nb,
                    register_t rarg1, register_t rarg2, register_t rarg3, void * carg) {
	ret_t ret = CCALLS(cb, cs, method_nb, rarg1, rarg2, rarg3, carg, NULL, NULL);
	return ret.cret;
}

register_t ccall_rrcc_r(void * cb, void * cs, int method_nb,
                    register_t rarg1, register_t rarg2, void * carg1, void * carg2) {
	ret_t ret = CCALLS(cb, cs, method_nb, rarg1, rarg2, 0, carg1, carg2, NULL);
	return ret.rret;
}
