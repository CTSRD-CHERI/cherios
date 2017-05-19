/*-
 * Copyright (c) 2011 Robert N. M. Watson
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

#include "boot/boot.h"
#include "stdio.h"
#include "uart.h"
#include "syscalls.h"
#include "plat.h"

/*
 * Provide a kernel-compatible version of printf, which invokes the UART
 * driver.
 */
void
uart_putchar(int c, void *arg __unused)
{

	uart_putc(c);
}

int
boot_vprintf(const char *fmt, va_list ap)
{

	return (kvprintf(fmt, uart_putchar, NULL, 10, ap));
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

/*
 * Various util functions
 */

void __boot_assert(const char *assert_function, const char *assert_file,
		   int assert_lineno, const char *assert_message) {
	boot_panic("assertion failure in %s at %s:%d: %s", assert_function,
		   assert_file, assert_lineno, assert_message);
}

void boot_vtrace(const char *context, const char *fmt, va_list ap) {
	boot_printf(KYLW KBLD"%s" KRST KYLW" - ", context);
	boot_vprintf(fmt, ap);
	boot_printf(KRST"\n");
}

void boot_trace(const char *context, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	boot_vtrace(context, fmt, ap);
	va_end(ap);
}

void boot_panic(const char *fmt, ...) {
	va_list ap;

	boot_printf(KMAJ"panic: ");
	va_start(ap, fmt);
	boot_vprintf(fmt, ap);
	va_end(ap);
	boot_printf(KRST"\n");

	hw_reboot();
}
