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
#include "object.h"
#include "string.h"
#include "uart.h"
#include "assert.h"
#include "stdio.h"
#include "queue.h"
#include "types.h"
#include "syscalls.h"
#include "init.h"
#include "namespace.h"
#include "elf.h"

/*Some "documentation" for the interface between the kernel and activation start                                        *
* These fields are setup by the caller of act_register                                                                  *
*                                                                                                                       *
* a0    : user GP argument (goes to main)                                                                               *
* c3    : user Cap argument (goes to main)                                                                              *
* c22   : runtime Cap argument (called self_cap or act_cap. Really an extra argument for special cases. goes to init)   *
*                                                                                                                       *
* These fields are setup by act_register itself. Although the queue is an argument to the function                      *
*                                                                                                                       *
* c21   : self control reference                                                 										*
* c23   : namespace reference (may be null for init and namespace)                                                      *
* c24   : kernel interface table                                                                                        *
* c25   : queue                                                                                                        */


static void * init_act_create(const char * name, void * c0, void * pcc, void * stack, queue_t * queue,
							  void * act_cap, register_t a0, capability c3) {
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

	frame.mf_a0 = a0;
	frame.cf_c3 = c3;

	return syscall_act_register(&frame, name, queue);
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

static cap_pair elf_loader(Elf_Env *env, const char * file, size_t * entry) {
	int filelen=0;
	capability addr = load(file, &filelen);
	if(!addr) {
		printf("Could not read file %s", file);
		return NULL_PAIR;
	}
	return elf_loader_mem(env, addr, NULL, NULL, entry);
}

static void *init_memcpy(void *dest, const void *src, size_t n) {
	return memcpy(dest, src, n);
}

void * load_module(module_t type, const char * file, register_t arg, capability carg) {

    size_t entry;
    Elf_Env env = {
            .alloc   = init_alloc,
            .free    = init_free,
            .printf  = printf,
            .vprintf = vprintf,
            .memcpy  = init_memcpy,
    };

	cap_pair prgmp = elf_loader(&env, file, &entry);

	if(!prgmp.data) {
		assert(0);
		return NULL;
	}

	size_t low_bits = cheri_getbase(prgmp.data) & (0x20 - 1);

	if(low_bits != 0) {
		printf("ERROR: alignment of loaded file %s was %ld\n", file, low_bits);
		assert(0);
	}

	size_t allocsize = cheri_getlen(prgmp.data);

	size_t stack_size = 0x10000;
	size_t stack_align = 0x40;
	size_t queue_size = ((sizeof(queue_default_t) + stack_align - 1) / stack_align) * stack_align;
	void * stack = init_alloc(stack_size).data;
	if(!stack) {
		assert(0);
		return NULL;
	}

	/* Steal a few bytes from the bottom of the stack to use as the message queue */
	queue_t* queue = (queue_t*)((char*)stack + stack_size - queue_size);

	queue = cheri_setbounds(queue, queue_size);
	stack = cheri_setbounds(stack, stack_size - queue_size);

	void * pcc = cheri_getpcc();
	pcc = cheri_setoffset(prgmp.code, entry);
	pcc = cheri_andperm(pcc, (CHERI_PERM_GLOBAL | CHERI_PERM_EXECUTE | CHERI_PERM_LOAD
                              | CHERI_PERM_LOAD_CAP));
	void * ctrl = init_act_create(file, cheri_setoffset(prgmp.data, 0),
	pcc, stack, queue, get_act_cap(type), arg, carg);
	if(ctrl == NULL) {
		return NULL;
	}

	return ctrl;
}

static int act_alive(capability ctrl) {
	if(!ctrl) {
		return 0;
	}

	status_e ret = SYSCALL_OBJ_void(syscall_act_ctrl_get_status, ctrl);

	if(ret == status_terminated) {
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
	return nb;
}

int num_registered_modules(void) {
	return namespace_get_num_services();
}