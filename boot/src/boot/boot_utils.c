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
#include "plat.h"
#include "boot/boot.h"
#include "boot/boot_info.h"
#include "object.h"
#include "string.h"
#include "uart.h"
#include "elf.h"

static void *kernel_alloc_mem(size_t _size) {
	/* The kernel is direct-mapped. */
	(void) _size;
	return NULL;
}

static void kernel_free_mem(void *addr) {
	/* no-op */
	(void)addr;
}

static void *boot_memcpy(void *dest, const void *src, size_t n) {
	return memcpy(dest, src, n);
}

static boot_info_t bi;
static Elf_Env env;

void init_elf_loader() {
  env.alloc   = kernel_alloc_mem;
  env.free    = kernel_free_mem;
  env.printf  = boot_printf;
  env.vprintf = boot_vprintf;
  env.memcpy  = boot_memcpy;
}

void load_kernel() {
	extern u8 __kernel_elf_start, __kernel_elf_end;
	size_t minaddr, maxaddr, entry;
	char *prgmp = elf_loader_mem(&env, &__kernel_elf_start,
				     &minaddr, &maxaddr, &entry);

	if(!prgmp) {
		boot_printf(KRED"Could not load kernel file"KRST"\n");
		goto err;
	}

	if(maxaddr > (size_t)(&__boot_load_virtaddr)) {
		boot_printf(KRED"Kernel too large: %lx > %lx"KRST"\n",
			    maxaddr, (size_t)(&__boot_load_virtaddr));
		goto err;
	}

	if(&__kernel_entry_point != prgmp + entry) {
		boot_printf(KRED"Bad kernel entry point:"KRST"\n");
		BOOT_PRINT_CAP(prgmp);
		boot_printf("Expected kernel entry point:\n");
		BOOT_PRINT_CAP(&__kernel_entry_point);
		goto err;
	}

	caches_invalidate(&__kernel_load_virtaddr,
	                  maxaddr - (size_t)(&__kernel_load_virtaddr));

	bzero(&bi, sizeof(bi));
	bi.kernel_start_addr = &__kernel_load_virtaddr;
	bi.kernel_mem_size = maxaddr - (size_t)(&__kernel_load_virtaddr);

	return;
err:
	hw_reboot();
}

#define	INIT_STACK_SIZE	0x10000
#define	PAGE_ALIGN	0x1000

static void *make_aligned_data_cap(const char *start, size_t len) {
	size_t desired_ofs = ((size_t)start + PAGE_ALIGN);
	desired_ofs &= ~ PAGE_ALIGN;

	char *cap = (char *)desired_ofs;
	return cap;
}

static void *make_free_mem_cap(const char *start) {
	char *cap  = (char *)(size_t)start;

	return cap;
}

boot_info_t *load_init() {
	extern u8 __init_elf_start, __init_elf_end;
	size_t minaddr, maxaddr, entry;

	// FIXME: init is direct mapped for now
	char *prgmp = elf_loader_mem(&env, &__init_elf_start,
				     &minaddr, &maxaddr, &entry);

	if(!prgmp) {
		boot_printf(KRED"Could not load init file"KRST"\n");
		goto err;
	}

	if(maxaddr > (size_t)(&__boot_load_virtaddr)) {
		boot_printf(KRED"Init too large: %lx > %lx"KRST"\n",
			    maxaddr, (size_t)(&__boot_load_virtaddr));
		goto err;
	}

	caches_invalidate(&__init_load_virtaddr,
	                  maxaddr - (size_t)(&__init_load_virtaddr));

	/* set up a stack region just after the loaded executable */
	void * stack = make_aligned_data_cap(prgmp + maxaddr, INIT_STACK_SIZE);

	/* free memory starts beyond this stack */
	bi.start_free_mem = make_free_mem_cap((char *)stack + INIT_STACK_SIZE);

	/* set up pcc */
	void *pcc = prgmp;

	/* populate frame */
	bzero(&bi.init_frame, sizeof(bi.init_frame));
	bi.init_frame.cf_pcc = pcc;
	bi.init_frame.mf_pc  = (size_t)pcc + entry;
	bi.init_frame.cf_c11 = stack;
	bi.init_frame.mf_sp  = (size_t)stack;
	bi.init_frame.cf_c12 = pcc;
	bi.init_frame.cf_c0  = 0;

	return &bi;
err:
	hw_reboot();
}

void install_exception_vector(void) {
	/* Copy exception trampoline to exception vector */
	char * all_mem = NULL;
	void *mips_bev0_exception_vector_ptr =
	                (void *)(all_mem + MIPS_BEV0_EXCEPTION_VECTOR);
	size_t nbytes = (char *)&kernel_trampoline_end - (char *)&kernel_trampoline;
	memcpy(mips_bev0_exception_vector_ptr, &kernel_trampoline, nbytes);
	cp0_status_bev_set(0);
}

void hw_init(void) {
	uart_init();
	cp0_hwrena_set(cp0_hwrena_get() | (1<<2));
}
