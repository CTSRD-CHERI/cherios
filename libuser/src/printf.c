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

#include "mips.h"
#include "stdarg.h"
#include "stdio.h"
#include "object.h"
#include "assert.h"
#include "namespace.h"

#define BUF_SIZE	0x100

static void buf_puts(char * str) {
	#if 1
	/* Syscall version */
	__asm__ __volatile__ (
		"li   $v0, 34 \n"
		"cmove $c3, %[str] \n"
		"syscall      \n"
		:: [str]"C" (str): "v0", "$c3");
	#else
	/* CCall version */
	static void * uart_ref = NULL;
	static void * uart_id  = NULL;
	if(uart_ref == NULL) {
		uart_ref = namespace_get_ref(1);
		uart_id  = namespace_get_id(1);
	}
	assert(uart_ref != NULL);
	assert(uart_id != NULL);
	ccall_c_n(uart_ref, uart_id, 1, str);
	#endif
}

void buf_putc(char chr) {
	static size_t offset = 0;
	static char buf[BUF_SIZE+1];
	buf[offset++] = chr;
	if((chr == '\n') || (offset == BUF_SIZE)) {
		buf[offset] = '\0';
		buf_puts(buf);
		offset = 0;
	}
}


/*
 * Provide a kernel-compatible version of printf, which invokes the UART
 * driver.
 */
static void
uart_putchar(int c, void *arg __unused)
{
	buf_putc(c);
}

int
vprintf(const char *fmt, va_list ap)
{
	return (kvprintf(fmt, uart_putchar, NULL, 10, ap));
}

int puts(const char *s) {
	while(*s) {
		uart_putchar(*s++, NULL);
	}
	uart_putchar('\n', NULL);
	return 0;
}

int putc(int character, FILE *f) {
	return fputc(character, f);
}

int fputc(int character, FILE *f) {
	if(f != NULL) {
		panic("fprintf not implememted");
	}
	buf_putc((unsigned char)character);
	return character;
}



int
printf(const char *fmt, ...)
{
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = vprintf(fmt, ap);
	va_end(ap);

	return (retval);
}

/* maps to printf */
int
fprintf(FILE *f, const char *fmt, ...)
{
	if(f != NULL) {
		panic("fprintf not implememted");
	}

	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = vprintf(fmt, ap);
	va_end(ap);

	return (retval);
}
