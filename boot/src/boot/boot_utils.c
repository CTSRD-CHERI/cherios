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
#include "plat.h"
#include "boot/boot.h"
#include "boot/boot_info.h"
#include "object.h"
#include "string.h"
#include "uart.h"
#include "elf.h"

#define TRACE_BOOT_LOADER	0

#if TRACE_BOOT_LOADER

#define TRACE_PRINT_PTR(ptr) BOOT_PRINT_PTR(ptr)
#define TRACE_PRINT_CAP(cap) BOOT_PRINT_CAP(cap)

#else

#define TRACE_PRINT_PTR(...)
#define TRACE_PRINT_CAP(...)

#endif

#define NANO_SIZE 64 * 1024 * 1024
#define K_ALLOC_ALIGN (0x10000)

static char* phy_mem;

static void *kernel_alloc_mem(size_t _size) {
	/* We will allocate the first few objects in low physical memory. THe first thing we load is the nano kernel
	 * and this will be direct mapped.*/
    static int alloc_direct = 1;
    capability alloc;

    if(alloc_direct) {
        if(_size > NANO_SIZE + MIPS_KSEG0) {
            boot_printf(KRED"nano kernel too large\n"KRST);
            hw_reboot();
        }
        boot_printf("Nano kernel size: %lx. Reserved: %lx\n", _size - MIPS_KSEG0, (unsigned long)NANO_SIZE);
        phy_mem =     cheri_setoffset(cheri_getdefault(), MIPS_KSEG0 + NANO_SIZE);
        alloc = cheri_getdefault();
        alloc_direct = 0;
    } else {
        alloc = phy_mem;
        size_t align_off = (K_ALLOC_ALIGN - (_size & (K_ALLOC_ALIGN-1)) & (K_ALLOC_ALIGN-1));
        phy_mem += _size + align_off;
    }

	return alloc;
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

capability load_nano() {
	extern u8 __nano_elf_start, __nano_elf_end;
	size_t minaddr, maxaddr, entry;
	char *prgmp = elf_loader_mem(&env, &__nano_elf_start,
								 &minaddr, &maxaddr, &entry);
    if(!prgmp) {
        boot_printf(KRED"Could not load nano kernel file"KRST"\n");
        goto err;
    }


	__asm__ ("sync");

	bi.nano_begin = 0;
	bi.nano_end = NANO_SIZE;

    boot_printf(KRED"Loaded nano kernel: minaddr=%lx maxaddr=%lx entry=%lx "KRST"\n",
                minaddr, maxaddr, entry);

    return prgmp + entry;

    err:
    hw_reboot();
}

size_t load_kernel() {
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

    bi.kernel_begin = (cheri_getoffset(prgmp) + cheri_getbase(prgmp)) - MIPS_KSEG0;
    bi.kernel_end = (cheri_getoffset(phy_mem) + cheri_getbase(phy_mem)) - MIPS_KSEG0;

	boot_printf(KRED"Loaded kernel: minaddr=%lx maxaddr=%lx entry=%lx "KRST"\n",
		    minaddr, maxaddr, entry);

	return entry;
err:
	hw_reboot();
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

	boot_printf(KRED"Loaded init: minaddr=%lx maxaddr=%lx entry=%lx "KRST"\n",
		    minaddr, maxaddr, entry);

    bi.init_begin = (cheri_getoffset(prgmp) + cheri_getbase(prgmp)) - MIPS_KSEG0;
    bi.init_end = (cheri_getoffset(phy_mem) + cheri_getbase(phy_mem)) - MIPS_KSEG0;
	return &bi;
err:
	hw_reboot();
}

void hw_init(void) {
	uart_init();
	cp0_hwrena_set(cp0_hwrena_get() | (1<<2));
}
