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

#include "boot/boot.h"
#include "klib.h"
#include "kernel.h"
#include "cp0.h"
#include "misc.h"
#include "object.h"
#include "string.h"
#include "statcounters.h"

statcounters_bank_t theCounterStart;
statcounters_bank_t theCounterEnd;
statcounters_bank_t theCounterDiff;

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
	void * c_memmgt = load_module(m_memmgt, "memmgt.elf",  0);
	nssleep(3);
	boot_alloc_enable_system(c_memmgt);

	void * c_ns = load_module(m_namespace, "namespace.elf", 0);
	nssleep(3);
	/* glue memmgt to namespace */
	glue_memmgt(c_memmgt, c_ns);

	load_module(m_uart, "uart.elf",		0);
	load_module(m_core, "sockets.elf",	0);
	//load_module(m_core, "zlib.elf",		0);
	nssleep(3);
	/* TODO: wait correctly for core modules */
	load_module(m_user, "prga.elf",		1);
	load_module(m_user, "prga.elf",		2);
	//load_module(m_user, "zlib_test.elf",	0);
}

int cherios_main(void) {
    /* Reset the statcounters */
    reset_statcounters();
    zero_statcounters(&theCounterStart);
    zero_statcounters(&theCounterEnd);
    zero_statcounters(&theCounterDiff);
    sample_statcounters(&theCounterStart);

	/* Init hardware */
	hw_init();

	kernel_printf("Hello world\n");

	/* Init bootloader */
	kernel_printf("B\n");
	boot_alloc_init();

	/* Print fs build date */
	kernel_printf("C\n");
	print_build_date();

	/* Init several kernel parts */
	kernel_printf("D\n");
	install_exception_vectors();
	act_init();

	/* Load modules */
	kernel_printf("E\n");
	load_modules();

	/* Start timer and activate interrupts */
	kernel_printf("Y\n");
	kernel_timer_init();
	/* Interrupts are ON from here */
	kernel_printf("Z\n");

	return 0;
}
