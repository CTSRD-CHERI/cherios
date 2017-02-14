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
#include "cheric.h"
#include "cp0.h"
#include "boot/boot.h"
#include "object.h"
#include "string.h"
#include "uart.h"
#include "assert.h"
#include "stdio.h"
#include "queue.h"
#include "types.h"
#include "syscalls.h"

static void * boot_act_register(reg_frame_t * frame, queue_t* queue, const char * name, register_t a0) {
	void * ret;
	__asm__ __volatile__ (
		"li    $v0, 20       \n"
		"cmove $c3, %[frame] \n"
		"cmove $c4, %[name]  \n"
		"cmove $c5, %[queue] \n"
		"move  $a0, %[a0]	 \n"
		"syscall             \n"
		"cmove %[ret], $c3   \n"
		: [ret] "=C" (ret)
		: [frame] "C" (frame), [queue] "C" (queue), [name] "C" (name), [a0] "r" (a0)
		: "v0", "$c3", "$c4", "$c5", "a0");
	return ret;
}

static void * boot_act_create(const char * name, void * c0, void * pcc, void * stack, queue_t * queue,
	                 void * act_cap, register_t a0) {
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

	/* set self cap */
	frame.cf_c22	= act_cap;

	capability ctrl = boot_act_register(&frame, queue, name, a0);
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

void * load_module(module_t type, const char * file, int arg) {
	char *prgmp = elf_loader(file, 0, NULL);
	if(!prgmp) {
		assert(0);
		return NULL;
	}
	size_t allocsize = cheri_getlen(prgmp);

	size_t stack_size = 0x10000;
	size_t stack_align = 0x40;
	size_t queue_size = ((sizeof(queue_default_t) + stack_align - 1) / stack_align) * stack_align;
	void * stack = boot_alloc(stack_size);
	if(!stack) {
		assert(0);
		return NULL;
	}

	/* Steal a few bytes from the bottom of the stack to use as the message queue */
	/* TODO, this is a bit hacky, but I guess this is only for boot programs */
	queue_t* queue = (queue_t*)((char*)stack + stack_size - queue_size);

	queue = cheri_setbounds(queue, queue_size);
	stack = cheri_setbounds(stack, stack_size - queue_size);

	void * pcc = cheri_getpcc();
	pcc = cheri_setbounds(cheri_setoffset(pcc, cheri_getbase(prgmp)) , allocsize);
	pcc = cheri_setoffset(pcc, cheri_getoffset(prgmp));
	pcc = cheri_andperm(pcc, 0b10111);
	void * ctrl = boot_act_create(file, cheri_setoffset(prgmp, 0),
	              pcc, stack, queue, get_act_cap(type), arg);
	if(ctrl == NULL) {
		return NULL;
	}
	return ctrl;
}

void load_kernel(const char * file) {
	size_t maxaddr = 0;
	char *prgmp = elf_loader(file, 1, &maxaddr);
	if(!prgmp) {
		boot_printf(KRED"Could not load kernel file"KRST"\n");
		goto err;
	}

	if(maxaddr > (size_t)(&__boot_load_virtaddr)) {
		boot_printf(KRED"Kernel too large: %lx > %lx"KRST"\n",
		    maxaddr, (size_t)(&__boot_load_virtaddr));
		goto err;
	}

	if(&__kernel_entry_point != prgmp) {
		boot_printf(KRED"Bad kernel entry point: %lx"KRST"\n", prgmp);
		goto err;
	}

	caches_invalidate(&__kernel_load_virtaddr,
	                  maxaddr - (size_t)(&__kernel_load_virtaddr));

	return;
	err:
	hw_reboot();
}

void install_exception_vector(void) {
	/* Copy exception trampoline to exception vector */
	char * all_mem = cheri_getdefault() ;
	void *mips_bev0_exception_vector_ptr =
	                (void *)(all_mem + MIPS_BEV0_EXCEPTION_VECTOR);
	memcpy(mips_bev0_exception_vector_ptr, &kernel_trampoline,
	    (char *)&kernel_trampoline_end - (char *)&kernel_trampoline);
	cp0_status_bev_set(0);
}

void hw_init(void) {
	uart_init();
	cp0_hwrena_set(cp0_hwrena_get() | (1<<2));
}

static int act_alive(capability ctrl) {
	if(!ctrl) {
		return 0;
	}
	status_e ret;
	SYSCALL_c3_retr(ACT_CTRL_GET_STATUS, ctrl, ret);

	if(ret == status_terminated) {
		return 0;
	}
	return 1;
}

int acts_alive(boot_elem_t * boot_list, size_t  boot_list_len) {
	int nb = 0;
	for(size_t i=0; i<boot_list_len; i++) {
		boot_elem_t * be = boot_list + i;
		if((!be->daemon) && act_alive(be->ctrl)) {
			nb++;
			break;
		}
	}
	//boot_printf(KRED"%d still alive\n", nb);
	return nb;
}
