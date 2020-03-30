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
#include "locale.h"

typedef unix_like_socket FILE;
extern __thread FILE * stderr;
extern __thread FILE * stdout;
extern __thread FILE * stdin; // Currently always NULL

#define EOF -1

__BEGIN_DECLS

typedef void kvprintf_putc_f (int,void*);
int	kvprintf(char const *fmt, void (*func)(int, void*), void *arg, int radix, va_list ap);
int	vsprintf(char *buf, const char *cfmt, va_list ap);
int	vsnprintf(char *str, size_t size, const char *format, va_list ap);
int	printf(const char *fmt, ...) __printflike(1, 2);
#ifdef USE_SYSCALL_PUTS
#define syscall_printf(...) printf(__VA_ARGS__)
#else
int	syscall_printf(const char *fmt, ...) __printflike(1, 2);
int syscall_vprintf(const char *fmt, va_list ap);
#endif
int	vprintf(const char *fmt, va_list ap);
int	fprintf(FILE * f, const char *fmt, ...) __printflike(2, 3);
int sprintf ( char * str, const char * format, ... );
int snprintf(char *str, size_t size, const char *format, ...);
int	puts(const char *s);
int	fputc(int character, FILE * stream);
static int putc(int ch, FILE *stream) {
    return fputc(ch,stream);
}
void	panic(const char *str) __dead2;
void panic_proxy(const char *str, act_kt act) __dead2;

int asprintf(char **strp, const char *fmt, ...);
int vasprintf(char **strp, const char *fmt, va_list ap);

typedef size_t fpos_t;

// Not yet implemented

int fclose(FILE *stream);
int fflush(FILE *stream);
void setbuf(FILE *__restrict stream, char *__restrict buffer);
int setvbuf(FILE *__restrict stream, char *__restrict buffer, int mode, size_t size);
int fscanf(FILE *__restrict stream, const char *__restrict format, ... );
int sscanf(const char *__restrict buffer, const char *__restrict format, ... );
int vfprintf(FILE *__restrict stream, const char *__restrict format, va_list vlist);
int vfscanf(FILE *__restrict stream, const char *__restrict format, va_list vlist);
int vsscanf(const char *__restrict buffer, const char *__restrict format, va_list vlist);
int fgetc(FILE *stream);
char *fgets(char *__restrict str, int count, FILE *__restrict stream);
int fputs(const char *__restrict str, FILE *__restrict stream);
int getc(FILE *stream);
int ungetc(int ch, FILE *stream);
size_t fread(void *__restrict buffer, size_t size, size_t count,
              FILE *__restrict stream);
size_t fwrite(const void *__restrict buffer, size_t size, size_t count,
               FILE *__restrict stream);
int fgetpos(FILE *__restrict stream, fpos_t *__restrict pos);
int fsetpos(FILE *stream, const fpos_t *pos);
int fseek(FILE *stream, long offset, int origin);
long ftell(FILE *stream);
void rewind(FILE *stream);
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void perror(const char *s);
int getchar(void);
int scanf(const char *__restrict format, ...);
int vscanf(const char *__restrict format, va_list vlist);
int putchar(int ch);

__END_DECLS
#endif /* !__STDIO_H__ */
