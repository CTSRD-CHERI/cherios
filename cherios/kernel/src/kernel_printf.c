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

// TODO: Eventually we want to remove the uart driver from the kernel, but it is very useful for debugging
#include "klib.h"
#ifndef __LITE__
int	kvprintf(char const *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap);

#include "uart.h"

/*
 * Provide a kernel-compatible version of printf and puts, which invokes the
 * UART driver.
 */
void kernel_puts(const char *s) {
	while(*s) {
		uart_putc(*s++);
	}
}

static void uart_putchar(int c, void *arg __unused) {
	uart_putc(c);
}

int kernel_vprintf(const char *fmt, va_list ap) {
	return (kvprintf(fmt, uart_putchar, NULL, 10, ap));
}

int kernel_printf(const char *fmt, ...) {
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = kernel_vprintf(fmt, ap);
	va_end(ap);

	return (retval);
}

#endif
