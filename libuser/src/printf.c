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
#include "cheristd.h"
#include "stdlib.c"

// If the uart driver is not up we can't write to stdout
// Instead we just use a syscall, currently the kernel has its own uart driver

// This will just collect data in a buffer until a \n or the buffer is full, then use syscall puts

#define CAN_PRINT 1

#ifdef USE_SYSCALL_PUTS

#if (GO_FAST)
    #undef CAN_PRINT
    #define CAN_PRINT 0
#endif

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
#if (GO_FAST)
    (void)fmt;
    return 0;
#else
    char buf[0x100];
    va_list ap;
    int retval;

    va_start(ap, fmt);
    retval = (kvprintf(fmt, NULL, buf, 10, ap));
    va_end(ap);

    syscall_puts(buf);

    return (retval);
#endif
}


int
syscall_vprintf(const char *fmt, va_list ap)
{
#if (GO_FAST)
    (void)fmt;
    (void)ap;
    return 0;
#else
    char buf[0x100];
    int ret = kvprintf(fmt, NULL, buf, 10, ap);
    syscall_puts(buf);
    return ret;
#endif
}

#endif

// This is the version we would like to use - it writes the character directly to the drb of a socket

#if (CAN_PRINT)
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

    needs_drb(f);

	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = (kvprintf(fmt, (kvprintf_putc_f*)fputc, f, 10, ap));
	va_end(ap);

	return (retval);
}

// People who use functions like this should be shot

int asprintf(char **strp, const char *fmt, ...) {

    va_list ap;
    int retval;

    va_start(ap, fmt);
    retval = vasprintf(strp, fmt, ap);
            va_end(ap);

    return (retval);
}

// Anything smaller than this will be formatted to the stack then copied into a correctly sized buffer
// Anything larger will end up in a buffer rounded to the nearest power of 2
#define ASPRINT_BUF_SIZE 0x200

typedef struct asprintf_state_s {
    // Each is double the size of the last. If it not the first the previous pointer is stashed at the start of the buffer
    char* buf;
    size_t cur_fill; // How full the current buffer is (all previous will be completely full)
    size_t cur_index; // Which buffer is currently being filled;
} asprintf_state_t;

static void asprintf_func(int ch, void* arg) {
    asprintf_state_t* state = (asprintf_state_t*)arg;

    if((state->cur_fill >= ASPRINT_BUF_SIZE) && is_power_2(state->cur_fill)) {
        char* next_buf = (char*)malloc(state->cur_fill * 2);
        *((char**)next_buf) = state->buf;
        state->buf = next_buf;
        state->cur_index++;
    }

    state->buf[state->cur_fill++] = (char)ch;
}

int vasprintf(char **strp, const char *fmt, va_list ap) {
    // We will eventually just keep doubling the target buffer and copy.
    // But start with the stack so that small string take up the exact size.
    char tmp_buf[ASPRINT_BUF_SIZE];
    asprintf_state_t state;
    state.buf = tmp_buf;
    state.cur_fill = 0;
    state.cur_index = 0;

    int retval = kvprintf(fmt, &asprintf_func, &state, 10, ap);

    // Pick which buffer to use
    size_t top_index = state.cur_index;
    char* final_s;

    if(top_index == 0) {
        final_s = malloc(state.cur_fill);
        memcpy(final_s, tmp_buf, state.cur_fill);
    } else {
        final_s = state.buf;
        char* buf = *((char**)final_s);

        size_t off = ASPRINT_BUF_SIZE << (size_t)(top_index - 2);

        // Condense and free extra buffers
        while(top_index != 1) {
            // Copy
            memcpy(final_s+off,buf+off,off);

            // Lift the previous buffer out
            char* prev_buf = *((char**)buf);

            // Free
            free(buf);

            // Adjust size
            off >>=1;

            // Go to next buffer
            buf = prev_buf;
            top_index--;
        }

        assert(buf == tmp_buf);
        // last buffer does not need freeing and is a slightly different size than the pattern would imply
        memcpy(final_s, buf, ASPRINT_BUF_SIZE);

    }

    *strp = final_s;
    return retval;
}

#else

int fputc(__unused int character, __unused FILE *f) {
    return 0;
}

int fputs(__unused const char* str, __unused FILE* f) {
    return 0;
}

int puts(__unused const char *s) {
    return 0;
}

int vprintf(__unused const char *fmt, __unused va_list ap) {
    return 0;
}

int printf(__unused const char *fmt, ...) {
    return 0;
}

int fprintf(__unused FILE *f, __unused const char *fmt, ...) {
    return 0;
}
#endif
