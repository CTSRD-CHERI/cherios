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

INCLUDE "common.ld"

__size_boot_stack	= 0x10000;	/* Start simple: 64K */

OUTPUT_FORMAT("elf64-littleriscv");
OUTPUT_ARCH(riscv)

PHDRS
{
	header PT_PHDR PHDRS ;
	boot PT_LOAD;
	text PT_LOAD;
	data PT_LOAD;
}

ENTRY(boot_start)
SECTIONS
{
	. = __boot_load_virtaddr_low;

	.boot : {
	    __boot_segment_start = .;
		*(.boot)
	} :boot = 0x0

    . = __boot_load_virtaddr;

    __cap_relocs ALIGN(CAP_ALIGN): {
        __text_segment_start = .;
        KEEP (*(__cap_relocs))
    } :text

    .rodata.str1.1 ALIGN(CAP_ALIGN): {
        KEEP *(.rodata.str1.1)
    } :text

	.text ALIGN(CAP_ALIGN): {
		*(.text)
	} :text

	.data ALIGN(CAP_ALIGN): {
        __data_segment_start = .;
		*(.data)
	} : data

	.bss ALIGN(CAP_ALIGN): {
		*(.bss)
	} : data

    __start_bss = ADDR(.bss);
    __stop_bss = ALIGN(__start_bss + SIZEOF(.bss), CAP_ALIGN);
    __bss_size = __stop_bss - __start_bss;

    /* Boots stack needs explicit allocation as we really don't want to do any dynamic allocation in boot */
	 __start_boot_stack = ALIGN(__stop_bss, PAGE_ALIGN);
     __stop_boot_stack  = __start_boot_stack + __size_boot_stack;

     /DISCARD/ :
     {
        *(.pdr)
        *(.comment)
        *(.got)
        *(.cap_table_local)
     }

}
