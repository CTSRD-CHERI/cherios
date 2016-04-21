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
#include "libuser.h"

void * module_register(int nb, int flags, void * data_cap, void * methods, int methods_nb, int * ret) {
	void * deleg_cap;
	__asm__ (
		"li    $v0, 5           \n"
		"move  $a0, %[nb]       \n"
		"move  $a1, %[flags]       \n"
		"move  $a2, %[methods_nb]       \n"
		"cmove $c3, %[methods]  \n"
		"cmove $c4, %[data_cap] \n"
		"syscall                \n"
		"cmove %[deleg_cap], $c3  \n"
		"move  %[ret], $v0  \n"
		: [ret]"=r" (*ret), [deleg_cap]"=C" (deleg_cap)
		: [nb]"r" (nb), [flags]"r" (flags),
		  [methods]"C" (methods), [methods_nb]"r" (methods_nb), [data_cap]"C" (data_cap)
		: "v0", "a0", "a1", "a2", "$c3", "$c4");
	return deleg_cap;
}

void ctor_null(void) {
	creturn_c(NULL);
}

void dtor_null(void) {
	creturn_n();
}

void * get_own_object(void) {
	void * object;
	__asm__ (
		"cmove %[object], $c26 \n"
		: [object]"=C" (object));
	return object;
}



void * get_kernel_object(void) {
	void * cb;
	__asm__ __volatile__ (
		"li   $v0, 99 \n"
		"syscall      \n"
		"cmove %[cb], $c3   \n"
		:[cb]"=C" (cb):);
	return cb;
}

void * get_kernel_methods(void) {
	void * methods;
	__asm__ __volatile__ (
		"li   $v0, 98 \n"
		"syscall      \n"
		"cmove %[methods], $c3   \n"
		:[methods]"=C" (methods)::"v0");
	return methods;
}

void * get_object(int module) {
	void * object;
	__asm__ (
		"cmove $c1, %[cs] \n"
		"cmove $c2, %[cb] \n"
		"move    $a0, %[module]     \n"
		"ccall $c1, $c2   \n"
		"cmove %[object], $c3   \n"
		:[object]"=C" (object)
		:[cs]"C" (libuser_kernel_methods[0]), [cb]"C" (libuser_kernel_cb),
		 [module]"r" (module)
		:"$c1","$c2","$c3");
	return object;
}

void ** get_methods(int module) {
	void * methods;
	__asm__ (
		"cmove $c1, %[cs] \n"
		"cmove $c2, %[cb] \n"
		"move    $a0, %[module]     \n"
		"ccall $c1, $c2 \n"
		"cmove %[methods], $c3   \n"
		:[methods]"=C" (methods)
		:[cs]"C" (libuser_kernel_methods[1]), [cb]"C" (libuser_kernel_cb),
		 [module]"r" (module)
		:"$c1","$c2","$c3");
	return methods;
}

/*
 * CCall helpers
 */
 
#define CCALL_CLOBS "$c1","$c2","$c3","v0","v1","a0","a1"
#define CCALL_ASM_CSCB "cmove $c1, %[cs] \n" "cmove $c2, %[cb] \n"
#define CCALL_ASM_RETC "cmove %[ret], $c3 \n"	: [ret]"=C" (ret)
#define CCALL_ASM_RETR "move %[ret], $v0 \n"	: [ret]"=r" (ret)

void ccall_c_n(void * cs, void * cb, const void * carg) {
	__asm__ __volatile__ (
		CCALL_ASM_CSCB
		"cmove $c3, %[carg] \n"
		"ccall $c1, $c2 \n"
		:
		: [cs]"C" (cs), [cb]"C" (cb), [carg]"C" (carg)
		: CCALL_CLOBS);
}

void * ccall_n_c(void * cs, void * cb) {
	void * ret;
	__asm__ __volatile__ (
		CCALL_ASM_CSCB
		"cfromptr  $c3,$c1,$zero \n"
		"ccall $c1, $c2 \n"
		CCALL_ASM_RETC
		: [cs]"C" (cs), [cb]"C" (cb)
		: CCALL_CLOBS);
	return ret;
}

void * ccall_r_c(void * cs, void * cb, int rarg) {
	void * ret;
	__asm__ __volatile__ (
		CCALL_ASM_CSCB
		"move $a0, %[rarg] \n"
		"ccall $c1, $c2 \n"
		CCALL_ASM_RETC
		: [cs]"C" (cs), [cb]"C" (cb), [rarg]"r" (rarg)
		: CCALL_CLOBS);
	return ret;
}

int ccall_n_r(void * cs, void * cb) {
	int ret;
	__asm__ __volatile__ (
		CCALL_ASM_CSCB
		"ccall $c1, $c2 \n"
		CCALL_ASM_RETR
		: [cs]"C" (cs), [cb]"C" (cb)
		: CCALL_CLOBS);
	return ret;
}

int ccall_r_r(void * cs, void * cb, int rarg) {
	int ret;
	__asm__ __volatile__ (
		CCALL_ASM_CSCB
		"move $a0, %[rarg] \n"
		"ccall $c1, $c2 \n"
		CCALL_ASM_RETR
		: [cs]"C" (cs), [cb]"C" (cb), [rarg]"r" (rarg)
		: CCALL_CLOBS);
	return ret;
}

int ccall_rr_r(void * cs, void * cb, int rarg, int rarg2) {
	int ret;
	__asm__ __volatile__ (
		CCALL_ASM_CSCB
		"move $a0, %[rarg] \n"
		"move $a1, %[rarg2] \n"
		"ccall $c1, $c2 \n"
		CCALL_ASM_RETR
		: [cs]"C" (cs), [cb]"C" (cb), [rarg]"r" (rarg), [rarg2]"r" (rarg2)
		: CCALL_CLOBS);
	return ret;
}

int ccall_rc_r(void * cs, void * cb, int rarg, void * carg) {
	int ret;
	__asm__ __volatile__ (
		CCALL_ASM_CSCB
		"move $a0, %[rarg] \n"
		"cmove $c3, %[carg] \n"
		"ccall $c1, $c2 \n"
		CCALL_ASM_RETR
		: [cs]"C" (cs), [cb]"C" (cb), [rarg]"r" (rarg), [carg]"C" (carg)
		: CCALL_CLOBS);
	return ret;
}



/*
 * CReturn helpers
 */
void creturn_n(void) {
	__asm__ (
		"li $v0, 0 \n"
		"li $v1, 0 \n"
		"cfromptr  $c3,$c1,$zero \n"
		"creturn      \n");
}

void creturn_r(register_t ret) {
	__asm__ __volatile__ (
		"move $v0, %[ret] \n"
		"li $v1, 0 \n"
		"cfromptr  $c3,$c1,$zero \n"
		"creturn      \n"
		::[ret]"r" (ret));
}

void creturn_c(void * ret) {
	__asm__ __volatile__ (
		"li $v0, 0 \n"
		"li $v1, 0 \n"
		"cmove  $c3, %[ret] \n"
		"creturn      \n"
		::[ret]"C" (ret));
}
