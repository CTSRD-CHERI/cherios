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

INCLUDE "riscv.ld"

/* I know RISCV caps are 0x10 aligned, but the generic CheriOS LD alignment is 0x20 for 256-bit legacy reasons */
CAP_ALIGN = 0x20;
PAGE_ALIGN = 0x1000;

/* Careful changing nano size, its hardcoded in the nano kernel still (as NANO_SIZE) */
__nano_size               = 0x04000000;
__nano_load_physaddr      = __uncached_base;
__boot_load_physaddr_low  = 0;
/* It is neater to have the nanokernel at the start of RAM, and have boot's memory reclaimed. To to this, the majority
   of boot needs to be much further in memory so it does not overwrite itself */
__boot_load_physaddr      = 0x08000000;

/* Everything but the nano kernel are pcc/default base relative. */

__kernel_load_virtaddr = 0x000000;
__init_load_virtaddr   = 0x000000;

__boot_load_virtaddr     = __uncached_base + __boot_load_physaddr;
__boot_load_virtaddr_low = __uncached_base + __boot_load_physaddr_low;
__nano_load_virtaddr     = __nano_load_physaddr;
