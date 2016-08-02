/*-
 * Copyright (c) 2011 Robert N. M. Watson
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

#include "boot/boot.h"
#include "stdio.h"
#include "uart.h"

static int syscall_print = 0;

void boot_printf_syscall_enable(void) {
	syscall_print = 1;
}

static void buf_puts(const char * str) {
	if(syscall_print == 0) {
		kernel_printf("%s", str);
	} else {
		__asm__ __volatile__ (
		"li   $v0, 34 \n"
		"cmove $c3, %[str] \n"
		"syscall      \n"
		:: [str]"C" (str): "v0", "$c3");
	}
}

static void buf_putc(int c, __attribute__((unused)) void *arg) {
	char chr = (char)c;
	static size_t offset;
	const size_t buf_size = 0x100;
	static char buf[buf_size+1];
	buf[offset++] = chr;
	if((chr == '\n') || (offset == buf_size)) {
		buf[offset] = '\0';
		buf_puts(buf);
		offset = 0;
	}
}

int boot_vprintf(const char *fmt, va_list ap) {

	return (kvprintf(fmt, buf_putc, NULL, 10, ap));
}

static int boot_printf2(const char *fmt, ...) {
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = boot_vprintf(fmt, ap);
	va_end(ap);

	return (retval);
}

int boot_printf(const char *fmt, ...) {
	va_list ap;
	int retval;

	boot_printf2(KWHT);
	va_start(ap, fmt);
	retval = boot_vprintf(fmt, ap);
	va_end(ap);
	boot_printf2(KRST);

	return (retval);
}
