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

#include "mips.h"
#include "uart.h"

void hw_init(void) {
    capability ucap = cheri_getdefault();
    ucap = cheri_setoffset(ucap, MIPS_XKPHYS_UNCACHED_BASE + uart_base_phy_addr);
    ucap = cheri_setbounds(ucap, uart_base_size);
    set_uart_cap(ucap);
    uart_init();
    __asm__ (
    "mfc0	$t0, $7\n"
    "ori	$t0, $t0, 1 << 2\n"
    "mtc0	$t0, $7\n"
    :
    :
    : "t0"
    );
}

static void
cp0_status_bev_set(int bev)
{
    register_t status;

    /* XXXRW: Non-atomic test-and-set. */

    __asm__ __volatile__ ("dmfc0 %0, $12" : "=r" (status));
    if (bev)
        status |= MIPS_CP0_STATUS_BEV;
    else
        status &= ~MIPS_CP0_STATUS_BEV;
    __asm__ __volatile__ ("dmtc0 %0, $12" : : "r" (status));
}

void boot_platform_init(void) {
    /* TODO: we could have some boot exception vectors if we want exception  handling in boot. */
    /* These should be in ROM as a part of the boot image (i.e. make a couple more dedicated sections */
    cp0_status_bev_set(0);
}
