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

#include "mips.h"
#include "uart.h"

/*
 * Routines for interacting with the gxemul test console.  Programming details
 * are a result of manually inspecting the source code for gxemul's
 * dev_cons.cc and dev_cons.h.
 *
 * Offsets of I/O channels relative to the base.
 */
#define	GXEMUL_CONS_BASE		0x10000000
#define	GXEMUL_PUTGETCHAR_OFF		0x00000000
#define	GXEMUL_CONS_HALT		0x00000010

/*
 * One-byte buffer as we can't check whether the UART is readable without
 * actually reading from it.
 */
static char	buffer_data;
static int	buffer_valid;

/*
 * Low-level read and write routines.
 */
static inline uint8_t
cons_data_read(void)
{

	return (mips_ioread_uint8(mips_phys_to_uncached(GXEMUL_CONS_BASE +
	    GXEMUL_PUTGETCHAR_OFF)));
}

static inline void
cons_data_write(uint8_t v)
{

	mips_iowrite_uint8(mips_phys_to_uncached(GXEMUL_CONS_BASE +
	    GXEMUL_PUTGETCHAR_OFF), v);
}

int
uart_writable(void)
{

	return (1);
}

int
uart_readable(void)
{
	uint32_t v;

	if (buffer_valid)
		return (1);
	v = cons_data_read();
	if (v != 0) {
		buffer_valid = 1;
		buffer_data = v;
	}
	return (0);
}

char
uart_read(void)
{

	while (!uart_readable());
	buffer_valid = 0;
	return (buffer_data);
}

void
uart_write(char ch)
{

	cons_data_write(ch);
}

void
uart_init(void)
{

	/* Nothing required. */
}
