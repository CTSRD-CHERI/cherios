/*-
 * Copyright (c) 2011 Robert N. M. Watson
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2018 Lawrence Esswood
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

#include <sockets.h>
#include "mips.h"
#include "stdarg.h"
#include "stdio.h"
#include "object.h"
#include "assert.h"
#include "namespace.h"
#include "unistd.h"

// If the uart driver is not up we can't write to stdout
// Instead we just use a syscall, currently the kernel has its own uart driver

// This will just collect data in a buffer until a \n or the buffer is full, then use syscall puts
#ifdef USE_SYSCALL_PUTS

#define BUF_SIZE	0x100
static size_t syscall_buf_offset = 0;
static char syscall_buf[BUF_SIZE + 1];

void buf_putc(char chr) {
    syscall_buf[syscall_buf_offset++] = chr;
	if((chr == '\n') || (syscall_buf_offset == BUF_SIZE)) {
        syscall_buf[syscall_buf_offset] = '\0';
        syscall_puts(syscall_buf);
        syscall_buf_offset = 0;
	}
}

#else

int
syscall_printf(const char *fmt, ...)
{
    char buf[0x100];
    va_list ap;
    int retval;

    va_start(ap, fmt);
    retval = (kvprintf(fmt, NULL, buf, 10, ap));
    va_end(ap);

    syscall_puts(buf);

    return (retval);
}


int
syscall_vprintf(const char *fmt, va_list ap)
{
    char buf[0x100];
    int ret = kvprintf(fmt, NULL, buf, 10, ap);
    syscall_puts(buf);
    return ret;
}

#endif

// This is the version we would like to use - it writes the character directly to the drb of a socket

int fputc(int character, FILE *f) {
#ifdef USE_SYSCALL_PUTS
    if(f == NULL) {
        buf_putc((unsigned char)character);
        return character;
    }
#endif
    // We fill up the drb but we don't create a request. We instead bump 'partial length' in the drb
    data_ring_buffer* drb = &f->write_copy_buffer;

    if(drb->requeste_ptr + drb->partial_length - drb->fulfill_ptr == drb->buffer_size) {
        // FIXME: This disrespects the DONT_WAIT flag
        __unused ssize_t bw = socket_requester_wait_all_finish(f->write.push_writer, 0);
        assert_int_ex(-bw, ==, 0);
    }

    assert(drb->requeste_ptr + drb->partial_length - drb->fulfill_ptr != drb->buffer_size);

    drb->buffer[(drb->requeste_ptr + drb->partial_length++) & (drb->buffer_size-1)] = (char)character;

    if(character == '\n' || (drb->requeste_ptr + drb->partial_length - drb->fulfill_ptr == drb->buffer_size)) {
        __unused ssize_t flush = socket_flush_drb(f);
        assert(flush >= 0 || flush == E_SOCKET_CLOSED);
    }

    return character;
}

int fputs(const char* str, FILE* f) {
    if(f) assert(f->con_type & CONNECT_PUSH_WRITE);

    while(*str) {
        fputc(*str++, f);
    }
    fputc('\n', f);
    return 0;
}

int puts(const char *s) {
    return fputs(s, stdout);
}

int
vprintf(const char *fmt, va_list ap)
{
	return (kvprintf(fmt, (kvprintf_putc_f*)fputc, stdout, 10, ap));
}

int
printf(const char *fmt, ...)
{
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = (kvprintf(fmt, (kvprintf_putc_f*)fputc, stdout, 10, ap));
	va_end(ap);

	return (retval);
}

int
fprintf(FILE *f, const char *fmt, ...)
{
    assert(f->con_type & CONNECT_PUSH_WRITE);
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = (kvprintf(fmt, (kvprintf_putc_f*)fputc, f, 10, ap));
	va_end(ap);

	return (retval);
}