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

#include "klib.h"

#define VIRT_TEST_BASE  0x100000
#define VIRT_TEST_SIZE   0x1000

capability virt_test_cap;

void kernel_platform_init(page_t* book) {
    virt_test_cap = get_phy_cap(book, VIRT_TEST_BASE, VIRT_TEST_SIZE, 0, 1);
}

enum sifive_shutdown_status_e {
    FINISHER_FAIL = 0x3333,
    FINISHER_PASS = 0x5555,
    FINISHER_RESET = 0x7777
};

static void hw_reset_sifive(enum sifive_shutdown_status_e status, uint16_t code) {
    uint32_t val = (uint32_t)status | ((uint32_t)code << 16);
    *((volatile uint32_t *)virt_test_cap) = val;
}

void hw_reboot(void) {
    hw_reset_sifive(FINISHER_RESET, 0);
    kernel_printf("RISCV: shutdown failed\n");
    for(;;);
}
