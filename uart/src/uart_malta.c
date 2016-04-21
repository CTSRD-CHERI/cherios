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
#include "uart.h"

extern void * uart_cap;

/*-
 * Routines for interacting with the MIPS Malta 16550 console UART.
 * Programming details from the specification "National Seminconductor
 * PC16550D Universal Asynchronous Receiver/Transmitter with FIFOs".
 *
 * Offsets of data and control registers relative to the base.  All registers
 * are 8-bit embedded in a 64-bit double word.
 */
#define	MALTA_UART_RXTX_OFF	0x00000000	/* Data receive/transmit. */
#define	MALTA_UART_IER_OFF	0x00000008	/* Interrupt enable. */
#define	MALTA_UART_IIFIFO_OFF	0x00000010	/* Interrupt identification. */
#define	MALTA_UART_LCR_OFF	0x00000018	/* Line control register. */
#define	MALTA_UART_MCR_OFF	0x00000020	/* Modem control register. */
#define	MALTA_UART_LSR_OFF	0x00000028	/* Line status register. */
#define	MALTA_UART_MSR_OFF	0x00000030	/* Line control register. */
#define	MALTA_UART_SCRATCH_OFF	0x00000038	/* Scratch register. */

/*
 * Initialisation value for the line control register (LCTRL).
 */
#define	MALTA_UART_LCTRL_VAL	0x03	/* 8, N, 1. */

/*
 * Initialisation value for the programmable baud generator.
 */
#define	MALTA_UART_BAUD_VAL	0x0A	/* 9600. */

/*
 * Initialisation value for the interrupt enable register (INTEN).
 */
#define	MALTA_UART_INTEN_VAL	0x00	/* No ints please, we're British. */

/*
 * Initialisation value for the modem control register (MCRTL).
 */
#define	MALTA_UARTA_MCTRL_VAL	0x01	/* Set DTR. */

/*
 * Field of the line status register (LSR).
 */
#define	MALTA_UART_LSR_DR	0x01	/* Data ready. */
#define	MALTA_UART_LSR_OE	0x02	/* Overrun error. */
#define	MALTA_UART_LSR_PE	0x04	/* Parity error. */
#define	MALTA_UART_LSR_FE	0x08	/* Framing error. */
#define	MALTA_UART_LSR_BI	0x10	/* Break interrupt. */
#define	MALTA_UART_LSR_THRE	0x20	/* Transmit holding register empty. */
#define	MALTA_UART_LSR_TEMT	0x40	/* Transmitter empty. */
#define	MALTA_UART_LSR_ERR	0x80	/* Parity, framing, or break. */

/*
 * Hard-coded gxemul console base address.
 */
#define	MALTA_UART_BASE	0x180003f8

/*
 * Low-level read and write register routines.
 */
static inline char
uart_data_receive(void)
{
	return (mips_cap_ioread_uint8(uart_cap, MALTA_UART_RXTX_OFF));
}

static inline void
uart_data_transmit(char ch)
{
	mips_cap_iowrite_uint8(uart_cap, MALTA_UART_RXTX_OFF, ch);
}

static inline uint8_t
uart_lsr_read(void)
{
	return (mips_cap_ioread_uint8(uart_cap, MALTA_UART_LSR_OFF));
}

static inline void
uart_lcr_set(uint8_t v)
{
	mips_cap_iowrite_uint8(uart_cap, MALTA_UART_LCR_OFF, v);
}

static inline void
uart_ier_set(uint8_t v)
{
	mips_cap_iowrite_uint8(uart_cap, MALTA_UART_IER_OFF, v);
}

static inline void
uart_mcr_set(uint8_t v)
{

	mips_cap_iowrite_uint8(uart_cap, MALTA_UART_MCR_OFF, v);
}

int
uart_writable(void)
{

	return ((uart_lsr_read() & MALTA_UART_LSR_THRE) != 0);
}

int
uart_readable(void)
{

	return ((uart_lsr_read() & MALTA_UART_LSR_DR) != 0);
}

char
uart_read(void)
{

	return (uart_data_receive());
}

void
uart_write(char ch)
{

	uart_data_transmit(ch);
}

void
uart_init(void)
{
	uint8_t v;

	/* Configure line control register. */
	v = 0;
	v |= 0x3;		/* 8-bit character length. */
	uart_lcr_set(v);

	/* Configure interrupt enable register. */
	v = 0;
	uart_ier_set(v);

	/* Configure modem control register. */
	v = 0;
	v |= 0x1;		/* Set DTR. */
	uart_mcr_set(v);
}
