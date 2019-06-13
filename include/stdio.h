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

#ifndef __STDIO_H__
#define __STDIO_H__

#include "sockets.h"
#include "mips.h"
#include "cdefs.h"
#include "stdarg.h"
#include "colors.h"
#include "types.h"

typedef unix_like_socket FILE;

__BEGIN_DECLS

extern __thread FILE * stderr;
extern __thread FILE * stdout;

typedef void kvprintf_putc_f (int,void*);
int	kvprintf(char const *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap);
int	vsprintf(char *buf, const char *cfmt, va_list ap);
int	vsnprintf(char *str, size_t size, const char *format, va_list ap);
int	printf(const char *fmt, ...) __printflike(1, 2);
#ifdef USE_SYSCALL_PUTS
#define syscall_printf(...) printf(__VA_ARGS__)
#else
int	syscall_printf(const char *fmt, ...) __printflike(1, 2);
#endif
int	vprintf(const char *fmt, va_list ap);
int	fprintf(FILE * f, const char *fmt, ...) __printflike(2, 3);
int sprintf ( char * str, const char * format, ... );
int snprintf(char *str, size_t size, const char *format, ...);
int	puts(const char *s);
#define putc(c,s) fputc(c,s)
int	fputc(int character, FILE * stream);
void	panic(const char *str) __dead2;
void panic_proxy(const char *str, act_kt act) __dead2;

__END_DECLS
#endif /* !__STDIO_H__ */
