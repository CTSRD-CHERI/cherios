/*-
 * Copyright (c) 2017 Lawrence Esswood
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

OUTPUT_FORMAT("elf64-tradbigmips");
OUTPUT_ARCH(mips)
ENTRY(nano_kernel_init)
SECTIONS
{
    . = __cached_exception_vector_xtlbrefill;

	.text ALIGN(CAP_ALIGN) : AT(__cached_exception_vector_xtlbrefill) {
        KEEP(*(.trampoline_tlb))
        . = __cached_exception_vector_other;
        KEEP (*(.trampoline_exc))
        . = __cached_exception_vector_ccall;
        KEEP (*(.trampoline_ccall))

	    . = __nano_load_virtaddr;
	    KEEP (*(.init))
	    KEEP(*(.text))
	} :text = 0

	.bss ALIGN(0x10000): { *(.bss) }

    /DISCARD/ :
    {
       *(.MIPS.options)
       *(.MIPS.abiflags)
       *(.pdr)
       *(.comment)
       *(.data)
       *(.got)
       *(.cap_table_local)
    }
}

PHDRS
{
	header PT_PHDR PHDRS ;
	text PT_LOAD AT (__cached_exception_vector_xtlbrefill);
	bss PT_LOAD;
}