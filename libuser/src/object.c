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
void * act_self_cap   = NULL;

void object_init(void * self_ctrl, void* self_cap) {
	assert(self_ctrl != NULL);
	act_self_ctrl = self_ctrl;
	act_self_ref  = act_ctrl_get_ref(self_ctrl);
	act_self_id   = act_ctrl_get_id(self_ctrl);

	act_self_cap = self_cap;
}

void * act_get_cap(void) {
	return act_self_cap;
}

void * act_ctrl_get_ref(void * ctrl) {
	void * ref;
	__asm__ (
		"li    $v1, 21      \n"
		"move $a0, %[ctrl] \n"
		"syscall            \n"
		"move %[ref], $a0  \n"
		: [ref]"=r" (ref)
		: [ctrl]"r" (ctrl)
		: "v0", "v1", "a0");
	return ref;
}

void * act_ctrl_get_id(void * ctrl) {
	void * ref;
	__asm__ (
		"li    $v1, 22      \n"
		"move $a0, %[ctrl] \n"
		"syscall            \n"
		"move %[ref], $a0  \n"
		: [ref]"=r" (ref)
		: [ctrl]"r" (ctrl)
		: "v0", "v1", "a0");
	return ref;
}

int act_ctrl_revoke(void * ctrl) {
	int ret;
	__asm__ (
		"li    $v1, 25      \n"
		"move $a0, %[ctrl] \n"
		"syscall            \n"
		"move %[ret], $v0   \n"
		: [ret]"=r" (ret)
		: [ctrl]"r" (ctrl)
		: "v0", "v1", "a0");
	return ret;
}

int act_ctrl_terminate(void * ctrl) {
	int ret;
	__asm__ (
		"li    $v1, 26      \n"
		"move $a0, %[ctrl] \n"
		"syscall            \n"
		"move %[ret], $v0   \n"
		: [ret]"=r" (ret)
		: [ctrl]"r" (ctrl)
		: "v0", "v1", "a0");
	return ret;
}

void * act_seal_id(void * id) {
	void * sid;
	__asm__ (
		"li    $v1, 29      \n"
		"move $a0, %[id]   \n"
		"syscall            \n"
		"move %[sid], $a0  \n"
		: [sid]"=r" (sid)
		: [id]"r" (id)
		: "v0", "v1", "a0");
	return sid;
}

void ctor_null(void) {
	return;
}

void dtor_null(void) {
	return;
}

/*
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
 */

/*
 * CCall helpers
 */

#define CCALL_ASM_CSCB "move $t0, %[cb] \n" "move $t1, %[cs] \n" "move $v0, %[method_nb] \n"
//#define CCALL_INSTR(n) "ccall $c1, $c2, " #n "\n"
#define CCALL_INSTR(n) \
        "li $v1, " #n "\n" \
        "syscall \n" \

#define CCALL_INOPS [cb]"r" (cb), [cs]"r" (cs), [method_nb]"r" (method_nb)
#define CCALL_CLOBS "v0","v1","a0","a1","a2","a3","t0","t1"
#define CCALL_TOP \
	register_t ret; \
	__asm__ __volatile__ ( \
		CCALL_ASM_CSCB \
		"move  $a0, %[rarg1] \n" \
		"move  $a1, %[rarg2] \n" \
		"move  $a2, %[rarg3] \n" \
		"move  $a3, %[rarg4] \n" \

#define CCALL_BOTTOM \
		"move  %[ret], $v0  \n" \
		: [ret]"=r" (ret) \
		: CCALL_INOPS, [rarg1]"r" (rarg1), [rarg2]"r" (rarg2), [rarg3]"r" (rarg3), [rarg4]"r" (rarg4) \
		: CCALL_CLOBS);

#define CCALLS(...) CCALL(4, __VA_ARGS__)

register_t ccall_1(void * cb, void * cs, int method_nb,
		  register_t rarg1, register_t rarg2, register_t rarg3, register_t rarg4) {
	CCALL_TOP
		CCALL_INSTR(1001)
	CCALL_BOTTOM
	return ret;
}

register_t ccall_2(void * cb, void * cs, int method_nb,
		  register_t rarg1, register_t rarg2, register_t rarg3, register_t rarg4) {
	CCALL_TOP
		CCALL_INSTR(1002)
	CCALL_BOTTOM
	return ret;
}

inline register_t ccall_4(void * cb, void * cs, int method_nb,
		  register_t rarg1, register_t rarg2, register_t rarg3, register_t rarg4) {
	CCALL_TOP
		CCALL_INSTR(1004)
	CCALL_BOTTOM
	return ret;
}


void ccall_r_n(void * cb, void * cs, int method_nb, register_t rarg1) {
	CCALLS(cb, cs, method_nb, rarg1, 0, 0, 0);
}
void ccall_rr_n(void * cb, void * cs, int method_nb, register_t rarg1, register_t rarg2) {
	CCALLS(cb, cs, method_nb, rarg1, rarg2, 0, 0);
}
void ccall_rrr_n(void * cb, void * cs, int method_nb, register_t rarg1, register_t rarg2, register_t rarg3) {
	CCALLS(cb, cs, method_nb, rarg1, rarg2, rarg3, 0);
}
void ccall_rrrr_n(void * cb, void * cs, int method_nb, register_t rarg1, register_t rarg2, register_t rarg3, register_t rarg4) {
	CCALLS(cb, cs, method_nb, rarg1, rarg2, rarg3, rarg4);
}

register_t ccall_n_r(void * cb, void * cs, int method_nb) {
	register_t ret = CCALLS(cb, cs, method_nb, 0, 0, 0, 0);
	return ret;
}

register_t ccall_r_r(void * cb, void * cs, int method_nb, register_t rarg1) {
	register_t ret = CCALLS(cb, cs, method_nb, rarg1, 0, 0, 0);
	return ret;
}

register_t ccall_rr_r(void * cb, void * cs, int method_nb, register_t rarg1, register_t rarg2) {
	register_t ret = CCALLS(cb, cs, method_nb, rarg1, rarg2, 0, 0);
	return ret;
}

register_t ccall_rrr_r(void * cb, void * cs, int method_nb, register_t rarg1, register_t rarg2, register_t rarg3) {
	register_t ret = CCALLS(cb, cs, method_nb, rarg1, rarg2, rarg3, 0);
	return ret;
}

register_t ccall_rrrr_r(void * cb, void * cs, int method_nb, register_t rarg1, register_t rarg2, register_t rarg3, register_t rarg4) {
	register_t ret = CCALLS(cb, cs, method_nb, rarg1, rarg2, rarg3, rarg4);
	return ret;
}
