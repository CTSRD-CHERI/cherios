/*-
 * Copyright (c) 2016 Robert N. M. Watson
 * Copyright (c) 2016 Hadrien Barral
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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

#include "klib.h"
#include "lib.h"
#include "kernel.h"
#include "uart.h"
#include "cp0.h"
#include "cp2.h"

void print_build_date(void) {
	int filelen=0;
	char * date = load("t1", &filelen);
	if(date == NULL) {
		printf("%s failed\n", __func__);
		return;
	}
	date[filelen-1] = '\0';
	printf("%s\n", date);
}

void load_modules(void) {
	elf_loader("uart.elf");
	elf_loader("sockets.elf");
	/* TODO: Have an init.elf wait for core modules to be ready 
	   so user sandboxes can assume they are */
	elf_loader("prga.elf");
	elf_loader("prga.elf");
}

int
cherios_main(void)
{
	uart_init();
	kernel_printf("Hello world\n");
	
	/* Copy exception trampoline to exception vector */
	char * all_mem = __builtin_memcap_global_data_get();
	void *mips_bev0_exception_vector_ptr =
	                (void *)(all_mem + MIPS_BEV0_EXCEPTION_VECTOR);
	memcpy(mips_bev0_exception_vector_ptr, &kernel_exception_trampoline,
	    (char *)&kernel_exception_trampoline_end - (char *)&kernel_exception_trampoline);
	void *mips_bev0_ccall_vector_ptr =
	                (void *)(all_mem + MIPS_BEV0_CCALL_VECTOR);
	memcpy(mips_bev0_ccall_vector_ptr, &kernel_exception_trampoline,
	    (char *)&kernel_exception_trampoline_end - (char *)&kernel_exception_trampoline);
	cp0_status_bev_set(0);
	
	kernel_printf("B\n");
	/* Init several kernel parts */
	kernel_proc_init();
	kernel_object_init();
	kernel_methods_init();
	kernel_printf("C\n");
	print_build_date();
	kernel_printf("D\n");
	/* Load modules */
	load_modules();
	kernel_printf("Y\n");
	/* Start timer and activate interrupts */
	kernel_timer_init();
	/* Interrupts are ON from here */
	kernel_printf("Z\n");
	
	ssleep(-1);
	return (0);
}
