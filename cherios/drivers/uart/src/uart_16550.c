/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lawrence Esswood
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include "uart.h"
#include "cheric.h"

// These are all 1 byte
#define RBR_OFFSET      0x00 // R Receive Buffer Register
#define THR_OFFSET      0x00 // W Transmitter Holding Register
#define IER_OFFSET      0x01 // RW Interrupt Enable Register
#define IIR_OFFSET      0x02 // R Interrupt Identification Register
#define FCR_OFFSET      0x02 // W FIFO Control Register
#define LCR_OFFSET      0x03 // RW Line Control Register
#define MCR_OFFSET      0x04 // RW Modem Control Register
#define LSR_OFFSET      0x05 // R Line Status Register
#define MSR_OFFSET      0x06 // R Modem Status Register
#define DLR_OFFSET_LSB  0x00 // RW Divisor Latch Register (LSB)
#define DLR_OFFSET_MSB  0x01 // RW Divisor Latch Register (MSB)

size_t uart_base_phy_addr = 0x10000000;
size_t uart_base_size = 0x100;

volatile uint8_t* uart_cap;

char uart_read(void) {
    return uart_cap[RBR_OFFSET];
}

uint8_t get_status(void) {
    return uart_cap[LSR_OFFSET];
}

int	uart_readable(void) {
    return ((get_status() & (1 << 0)) != 0);
}

int	uart_writable(void) {
    return ((get_status() & (1 << 5)) != 0);
}

void uart_write(char ch) {
    uart_cap[THR_OFFSET] = (uint8_t)ch;
}

void set_uart_cap(capability cap) {
    uart_cap = (uint8_t*)cap;
}

void uart_init(void)
{
    // Disable interrupts, we will just poll, I don't care.
    uart_cap[IER_OFFSET] = 0;
    // So, the UART may have already been set up by the BIOS. In which case, do nothing?
}
