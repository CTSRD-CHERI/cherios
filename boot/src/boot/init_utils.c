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
#include "cheric.h"
#include "cp0.h"
#include "boot/boot.h"
#include "init.h"
#include "object.h"
#include "string.h"
#include "uart.h"
#include "assert.h"
#include "stdio.h"

static void * init_act_register(reg_frame_t * frame, const char * name) {
	void * ret;
	__asm__ __volatile__ (
		"li    $v0, 20       \n"
		"cmove $c3, %[frame] \n"
		"cmove $c4, %[name] \n"
		"syscall             \n"
		"cmove %[ret], $c3   \n"
		: [ret] "=C" (ret)
		: [frame] "C" (frame), [name] "C" (name)
		: "v0", "$c3", "$c4");
	return ret;
}

static void * init_act_create(const char * name, void * c0, void * pcc, void * stack,
	                 void * act_cap, void * ns_ref, void * ns_id, register_t a0) {
	reg_frame_t frame;
	memset(&frame, 0, sizeof(reg_frame_t));

	/* set pc */
	frame.cf_pcc	= pcc;
	frame.mf_pc	= cheri_getoffset(pcc);

	/* set stack */
	frame.cf_c11	= stack;
	frame.mf_sp	= cheri_getlen(stack);

	/* set c12 */
	frame.cf_c12	= frame.cf_pcc;

	/* set c0 */
	frame.cf_c0	= c0;

	/* set cap */
	frame.cf_c22	= act_cap;

	/* set namespace */
	frame.cf_c23	= ns_ref;
	frame.cf_c24	= ns_id;

	void * ctrl = init_act_register(&frame, name);
	CCALL(1, act_ctrl_get_ref(ctrl), act_ctrl_get_id(ctrl), 0,
	      a0, 0, 0, NULL, NULL, ctrl);
	return ctrl;
}

/* Return the capability needed by the activation */
static void * get_act_cap(module_t type) {
	void * cap = NULL;
	switch(type) {
		case m_uart:{}
			#ifdef CONSOLE_malta
				#define	UART_BASE	0x180003f8
				#define	UART_SIZE	0x40
			#elif defined(CONSOLE_altera)
				#define	UART_BASE	0x7f000000
				#define	UART_SIZE	0x08
			#else
			#error UART type not found
			#endif
			cap = cheri_getdefault();
			cap = cheri_setoffset(cap,
			    mips_phys_to_uncached(UART_BASE));
			cap = cheri_setbounds(cap, UART_SIZE);
			break;
		case m_memmgt:{}
			size_t heaplen = (size_t)&__stop_heap - (size_t)&__start_heap;
			void * heap = cheri_setoffset(cheri_getdefault(), (size_t)&__start_heap);
			heap = cheri_setbounds(heap, heaplen);
			cap = cheri_andperm(heap, 0b1111101 | CHERI_PERM_SOFT_1);
			break;
		case m_fs:{}
			void * mmio_cap = cheri_setoffset(cheri_getdefault(), mips_phys_to_uncached(0x1e400000));
			cap = cheri_setbounds(mmio_cap, 0x200);
			break;
		case m_namespace:
		case m_core:
		case m_user:
		case m_fence:
		default:{}
	}
	return cap;
}

static void * ns_ref = NULL;
static void * ns_id  = NULL;

void * load_module(module_t type, const char * file, int arg) {
	char *prgmp = elf_loader(file, &init_alloc, &init_free, NULL);
	if(!prgmp) {
		assert(0);
		return NULL;
	}

	/* Invalidate the whole range; elf_loader only returns a
	   pointer to the entry point. */
	size_t allocsize = cheri_getlen(prgmp);
	caches_invalidate(cheri_setoffset(prgmp, 0), allocsize);

	size_t stack_size = 0x10000;
	void * stack = init_alloc(stack_size);
	if(!stack) {
		assert(0);
		return NULL;
	}
	void * pcc = cheri_getpcc();
	pcc = cheri_setbounds(cheri_setoffset(pcc, cheri_getbase(prgmp)) , allocsize);
	pcc = cheri_setoffset(pcc, cheri_getoffset(prgmp));
	pcc = cheri_andperm(pcc, 0b10111);
	void * ctrl = init_act_create(file, cheri_setoffset(prgmp, 0),
	              pcc, stack, get_act_cap(type), ns_ref, ns_id, arg);
	if(ctrl == NULL) {
		return NULL;
	}
	if(type == m_namespace) {
		ns_ref = act_ctrl_get_ref(ctrl);
		ns_id = act_ctrl_get_id(ctrl);
	}
	return ctrl;
}

static int act_alive(void * ctrl) {
	if(!ctrl) {
		return 0;
	}
	int ret;
	__asm__ __volatile__ (
		"li    $v0, 23      \n"
		"cmove $c3, %[ctrl] \n"
		"syscall            \n"
		"move %[ret], $v0   \n"
		: [ret] "=r" (ret)
		: [ctrl] "C" (ctrl)
		: "v0", "$c3");
	if(ret == 2) {
		return 0;
	}
	return 1;
}

int acts_alive(init_elem_t * init_list, size_t  init_list_len) {
	int nb = 0;
	for(size_t i=0; i<init_list_len; i++) {
		init_elem_t * be = init_list + i;
		if((!be->daemon) && act_alive(be->ctrl)) {
			nb++;
			break;
		}
	}
	//boot_printf(KRED"%d still alive\n", nb);
	return nb;
}
