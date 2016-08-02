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
#include "cp0.h"
#include "misc.h"
#include "object.h"
#include "string.h"

boot_elem_t boot_list[] = {
	{m_memmgt,	"memmgt.elf",		0, 1, 0, NULL},
	{m_namespace,	"namespace.elf",	0, 1, 0, NULL},
	{m_uart,	"uart.elf",		0, 1, 0, NULL},
	{m_core,	"sockets.elf",		0, 1, 0, NULL},
	{m_core,	"zlib.elf",		0, 1, 0, NULL},
	{m_core,	"virtio-blk.elf",	0, 1, 0, NULL},
	{m_fence,	NULL,			0, 0, 0, NULL},
	{m_fs,		"fatfs.elf",		0, 0, 0, NULL},
	{m_fence,	NULL,			0, 0, 0, NULL},
	{m_user,	"prga.elf",		1, 0, 0, NULL},
	{m_user,	"prga.elf",		2, 0, 0, NULL},
	{m_user,	"zlib_test.elf",	0, 0, 0, NULL},
	{m_user,	"hello.elf",		0, 0, 0, NULL},

	{m_fence,	NULL,			0, 0, 0, NULL}
};

const size_t boot_list_len = countof(boot_list);

void print_build_date(void) {
	int filelen=0;
	char * date = load("t1", &filelen);
	if(date == NULL) {
		boot_printf("%s failed\n", __func__);
		return;
	}
	date[filelen-1] = '\0';
	boot_printf("%s\n", date);
}

static void load_modules(void) {
	static void * c_memmgt = NULL;

	for(size_t i=0; i<boot_list_len; i++) {
		boot_elem_t * be = boot_list + i;
		if(be->type == m_fence) {
			nssleep(3);
			continue;
		}
		be->ctrl = load_module(be->type, be->name, be->arg);
		switch(boot_list[i].type) {
			case m_memmgt:
				nssleep(3);
				c_memmgt = be->ctrl;
				boot_alloc_enable_system(be->ctrl);
				break;
			case m_namespace:
				nssleep(3);
				/* glue memmgt to namespace */
				glue_memmgt(c_memmgt, be->ctrl);
				break;
			default:{}
		}
	}
}

int cherios_main(void) {
	/* Init hardware */
	hw_init();

	boot_printf("Hello world\n");

	/* Init bootloader */
	boot_printf("B\n");
	stats_init();
	boot_alloc_init();

	/* Print fs build date */
	boot_printf("C\n");
	print_build_date();

	/* Load and init kernel */
	boot_printf("D\n");
	load_kernel("kernel.elf");
	install_exception_vector();
	__asm__ __volatile__ (
		"li    $v0, 0        \n"
		"syscall             \n"
		::: "v0");
	/* Interrupts are ON from here */
	boot_printf("E\n");

	/* Switch to syscall print */
	boot_printf_syscall_enable();

	/* Load modules */
	boot_printf("F\n");
	load_modules();

	boot_printf("Z\n");

	while(acts_alive(boot_list, boot_list_len)) {
		ssleep(0);
	}

	boot_printf(KBLD"Only daemons are alive. System shutown."KRST"\n");
	stats_display();
	hw_reboot();

	return 0;
}
