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
#include "cp0.h"
#include "plat.h"
#include "init.h"
#include "object.h"
#include "string.h"
#include "uart.h"
#include "assert.h"
#include "stdio.h"
#include "elf.h"

static void * init_act_register(reg_frame_t * frame, const char * name) {
	void * ret;
	__asm__ __volatile__ (
		"li    $v1, 20       \n"
		"move $a0, %[frame] \n"
		"move $a1, %[name]  \n"
		"syscall             \n"
		"move %[ret], $a0   \n"
		: [ret] "=r" (ret)
		: [frame] "r" (frame), [name] "r" (name)
		: "v0", "v1", "a0", "a1");
	return ret;
}

static void * init_act_create(const char * name, void * c0, void *pcbase, void * pcc, void * stack,
			      void * act_cap, void * ns_ref, void * ns_id,
			      register_t rarg, const void * carg) {
	reg_frame_t frame;
	memset(&frame, 0, sizeof(reg_frame_t));

	/* set pc */
	frame.mf_pc	= (register_t)pcc;

	/* set stack */
	frame.mf_sp	= (register_t)stack;

	/* set c12 */

	/* set c0 */

	/* set cap */
	frame.mf_s5	= (register_t)act_cap;

	/* set namespace */
	frame.mf_s6	= (register_t)ns_ref;
	frame.mf_s7	= (register_t)ns_id;

    /* remember pc for PIC */
	frame.mf_s4	= (register_t)pcbase;

	void * ctrl = init_act_register(&frame, name);
	CCALL(1, act_ctrl_get_ref(ctrl), act_ctrl_get_id(ctrl), 0,
	      rarg, (register_t)carg, 0, (register_t)ctrl);
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

        /*
		cap = cheri_getdefault();
		cap = cheri_setoffset(cap, mips_phys_to_uncached(UART_BASE));
		cap = cheri_setbounds(cap, UART_SIZE);
         */
		break;
	case m_memmgt:{}
        /* heap length not passed in the MIPS case
        if(caplen)
            caplen = (size_t)&__stop_heap - (size_t)&__start_heap;
         */
		void * heap = &__start_heap;
        *(size_t *)((size_t)heap + 0x100) = &__stop_heap - &__start_heap; // super ugly hack, put the length of the heap at heap + 0x100
        printf("Size of the heap: 0x%lx\n", &__stop_heap - &__start_heap);
        cap = heap;
        /*
		heap = cheri_setbounds(heap, heaplen);
		cap = cheri_andperm(heap, (CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_STORE
					   | CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE_CAP
					   | CHERI_PERM_STORE_LOCAL_CAP | CHERI_PERM_SOFT_1));
         */
		break;
	case m_fs:{}
		void * mmio_cap = (void *)mips_phys_to_uncached(0x1e400000);
        cap = mmio_cap;
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

static void * elf_loader(Elf_Env *env, const char * file, size_t *maxaddr, size_t * entry) {
	int filelen=0;
	char * addr = load(file, &filelen);
	if(!addr) {
		printf("Could not read file %s", file);
		return NULL;
	}
	return elf_loader_mem(env, addr, NULL, maxaddr, entry);
}

static void *init_memcpy(void *dest, const void *src, size_t n) {
	return memcpy(dest, src, n);
}

void * load_module(module_t type, const char * file, int arg, const void *carg) {
	size_t entry;
    size_t allocsize;
	Elf_Env env = {
	  .alloc   = init_alloc,
	  .free    = init_free,
	  .printf  = printf,
	  .vprintf = vprintf,
	  .memcpy  = init_memcpy,
	};

	char *prgmp = elf_loader(&env, file, &allocsize, &entry);
    printf("Module loaded at %p, entry: %lx\n", prgmp, entry);
	if(!prgmp) {
		assert(0);
		return NULL;
	}

	/* Invalidate the whole range; elf_loader only returns a
	   pointer to the entry point. */
	caches_invalidate(prgmp, allocsize);

	size_t stack_size = 0x10000;
	void * stack = init_alloc(stack_size);
	if(!stack) {
		assert(0);
		return NULL;
	}
	void * pcc = (void *)((size_t)prgmp + entry);
	void * ctrl = init_act_create(file, 0, prgmp,
				      pcc, (void *)((size_t)stack + stack_size), get_act_cap(type),
				      ns_ref, ns_id, arg, carg);
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
		"li    $v1, 23      \n"
		"move $a0, %[ctrl] \n"
		"syscall            \n"
		"move %[ret], $v0   \n"
		: [ret] "=r" (ret)
		: [ctrl] "r" (ctrl)
		: "v0", "v1", "a0");
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
	//printf(KRED"%d still alive\n", nb);
	return nb;
}
