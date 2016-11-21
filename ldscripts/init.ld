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
__size_init_stack	= 0x10000;	/* Start simple: 64K */

CAP_ALIGN = 0x20;
PAGE_ALIGN = 0x1000;

OUTPUT_FORMAT("elf64-tradbigmips");
OUTPUT_ARCH(mips)
ENTRY(start)
SECTIONS
{
	. += __init_load_virtaddr;
	. += SIZEOF_HEADERS;

	.init : {
		KEEP (*(.init))
	} = 0
	.text ALIGN(CAP_ALIGN): {
		*(.text)
	}
	.fini ALIGN(CAP_ALIGN): {
		KEEP (*(.fini))
	} = 0

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
	.preinit_array ALIGN(CAP_ALIGN) : {
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	}
	.init_array ALIGN(CAP_ALIGN) : {
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array))
		PROVIDE_HIDDEN (__init_array_end = .);
	}
	.fini_array ALIGN(CAP_ALIGN) : {
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(.fini_array))
		KEEP (*(SORT(.fini_array.*)))
		PROVIDE_HIDDEN (__fini_array_end = .);
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
	__stop_heap = ALIGN((__init_load_virtaddr + __size_memory) - __size_init_stack, PAGE_ALIGN);
	__size_heap = __stop_heap - __start_heap;

	__start_init_stack	= __stop_heap;
	__stop_init_stack	= __start_init_stack + __size_init_stack;
	__rest_of_memory	= __stop_init_stack;
}
