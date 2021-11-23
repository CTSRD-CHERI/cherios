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

__size_exception_stack	= 0x4000;	/* Start simple: 4K */

OUTPUT_FORMAT("elf64-littleriscv");
OUTPUT_ARCH(riscv)

/* These need to be specified manually as the indexs are hardcoded in init.c */
PHDRS
{
	text PT_LOAD;
	data PT_LOAD;
    header PT_PHDR PHDRS;
}

ENTRY(start)
SECTIONS
{
	. += SIZEOF_HEADERS;

    __text_segment_start = .;

	.text ALIGN(CAP_ALIGN): {
        KEEP (*(.init))
		*(.text)
	} : text
	.fini ALIGN(CAP_ALIGN): {
		KEEP (*(.fini))
	} : text = 0

	/*
	 * Memory to be mapped into the data segment.  For compiler-generated
	 * content, it would be much preferable to map into the code segment
	 * and use $pcc-relative loads, but we are not yet there.
	 */


	.data ALIGN(CAP_ALIGN): {
	    __data_segment_start = .;
		*(.data)
	} : data
    .captable : {
        *(.captable)
    } : data
	.bss ALIGN(CAP_ALIGN): {
		*(.bss)
	} : data

    __start_bss = ADDR(.bss);
    __stop_bss = ALIGN(__start_bss + SIZEOF(.bss), CAP_ALIGN);
    __bss_size = __stop_bss - __start_bss;

	.stack ALIGN(PAGE_ALIGN): {
		__start_exception_stack	= .;
		. += __size_exception_stack;
		__stop_exception_stack	= .;
	} : data

	/* todo: do not discard in debug mode */
	/DISCARD/ :
	{
	   *(.comment)
	   *(.tbss)
	   *(.tdata)
	   *(.cap_table_local)
	}
}
