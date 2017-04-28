/*-
 * Copyright (c) 2011-2016 Robert N. M. Watson
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

INCLUDE "common.ld"

__size_memory		= 0x1000000;	/* Start simple: 16M above load address */
__size_boot_stack	= 0x10000;	/* Start simple: 64K */

OUTPUT_FORMAT("elf64-tradbigmips");
OUTPUT_ARCH(mips)

PHDRS
{
	header PT_PHDR PHDRS ;
	boot PT_LOAD FILEHDR PHDRS AT (0xffffffff80100000) FLAGS(0x7) ;
	text PT_LOAD ;
}

ENTRY(beri_boot_start)
SECTIONS
{
	. += __beri_load_virtaddr;
	. += SIZEOF_HEADERS;

	.boot ALIGN(CAP_ALIGN): {
		*(.boot)
	} :boot = 0x0

	. = __boot_load_virtaddr;
	.text ALIGN(CAP_ALIGN): {
		*(.text)
	} :text

	/*
	 * Memory to be mapped into the data segment.  For compiler-generated
	 * content, it would be much preferable to map into the code segment
	 * and use $pcc-relative loads, but we are not yet there.
	 */
	.MIPS.options ALIGN(PAGE_ALIGN): {
		*(.MIPS.options)
	}
	.ctors ALIGN(CAP_ALIGN) : {
		KEEP (*/crtbeginC.o(.ctors))
		KEEP (*(EXCLUDE_FILE (*/crtendC.o) .ctors))
		KEEP (*(SORT(.ctors.*)))
		KEEP (*/crtendC.o(.ctors))
	}
	.dtors ALIGN(CAP_ALIGN) : {
		KEEP (*/crtbeginC.o(.dtors))
		KEEP (*(EXCLUDE_FILE (*/crtendC.o) .dtors))
		KEEP (*(SORT(.dtors.*)))
		KEEP (*/crtendC.o(.dtors))
	}
	.data ALIGN(CAP_ALIGN): {
		*(.data)
	}
	_gp = ALIGN(CAP_ALIGN) + 0x7fc0;
	.got : {
		*(.got.plt)
		*(.got)
	}
	.bss ALIGN(CAP_ALIGN): {
		*(.bss)
	}

	__start_bss = ADDR(.bss);
	__stop_bss = ALIGN(__start_bss + SIZEOF(.bss), CAP_ALIGN);

	/*
	 * No need to include anything after the following in the binary --
	 * we just define symbols, and don't move '.'.
	 */
	__start_heap = ALIGN(__stop_bss, PAGE_ALIGN);
	__stop_heap = ALIGN((__boot_load_virtaddr + __size_memory) - __size_boot_stack, PAGE_ALIGN);
	__size_heap = __stop_heap - __start_heap;

	__start_boot_stack	= __stop_heap;
	__stop_boot_stack	= __start_boot_stack + __size_boot_stack;
	__rest_of_memory	= __stop_boot_stack;
}
