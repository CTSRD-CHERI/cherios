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

#include "boot/boot.h"

/*
 * Various util functions
 */

void __kernel_assert(const char *assert_function, const char *assert_file,
			int assert_lineno, const char *assert_message) {
	kernel_panic("assertion failure in %s at %s:%d: %s", assert_function,
			assert_file, assert_lineno, assert_message);
}

void kernel_vtrace(const char *context, const char *fmt, va_list ap) {
	kernel_printf(KYLW KBLD"%s" KRST KYLW" - ", context);
	kernel_vprintf(fmt, ap);
	kernel_printf(KRST"\n");
}

void kernel_trace(const char *context, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	kernel_vtrace(context, fmt, ap);
	va_end(ap);
}

void kernel_error(const char *file, const char *func, int line, const char *fmt, ...) {
	kernel_printf(KRED "Kernel error: '");
	va_list ap;
	va_start(ap, fmt);
	kernel_vprintf(fmt, ap);
	va_end(ap);
	kernel_printf("' in %s, %s(), L%d"KRST"\n", file, func, line);
}

void kernel_panic(const char *fmt, ...) {
	va_list ap;

	kernel_printf(KMAJ"panic: ");
	va_start(ap, fmt);
	kernel_vprintf(fmt, ap);
	va_end(ap);
	kernel_printf(KRST"\n");

	hw_reboot();
}

void hw_reboot(void) {
	#ifdef HARDWARE_qemu
		/* Used to quit Qemu */
		mips_iowrite_uint8(mips_phys_to_uncached(0x1f000000 + 0x00500), 0x42);
	#endif
	for(;;);
}
