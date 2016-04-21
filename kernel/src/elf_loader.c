/*-
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
#include "klib.h"
#include "lib.h"
#include "math.h"

#define TRACE kernel_trace_elf_loader
#define ERROR TRACE
void kernel_trace_elf_loader(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	KERNEL_VTRACE("elf_loader", fmt, ap);
	va_end(ap);
}

typedef uint16_t Elf64_Half;	// Unsigned half int
typedef uint64_t Elf64_Off;	// Unsigned offset
typedef uint64_t Elf64_Addr;	// Unsigned address
typedef uint32_t Elf64_Word;	// Unsigned int
typedef int32_t  Elf64_Sword;	// Signed int
typedef uint64_t Elf64_Xword;	// Unsigned int
typedef int64_t  Elf64_Sxword;	// Signed int

typedef  struct
{
	unsigned char	e_ident[16];	/*  ELF  identification  */
	Elf64_Half	e_type;		/*  Object  file  type  */
	Elf64_Half	e_machine;	/*  Machine  type  */
	Elf64_Word	e_version;	/*  Object  file  version  */
	Elf64_Addr	e_entry;	/*  Entry  point  address  */
	Elf64_Off	e_phoff;	/*  Program  header  offset  */
	Elf64_Off	e_shoff;	/*  Section  header  offset  */
	Elf64_Word	e_flags;	/*  Processor-specific  flags  */
	Elf64_Half	e_ehsize;	/*  ELF  header  size  */
	Elf64_Half	e_phentsize;	/*  Size  of  program  header  entry  */
	Elf64_Half	e_phnum;	/*  Number  of  program  header  entries  */
	Elf64_Half	e_shentsize;	/*  Size  of  section  header  entry  */
	Elf64_Half	e_shnum;	/*  Number  of  section  header  entries  */
	Elf64_Half	e_shstrndx;	/*  Section  name  string  table  index  */
}  Elf64_Ehdr;

enum Elf_Ident {
	EI_MAG0		= 0, /* 0x7F */
	EI_MAG1		= 1, /* 'E' */
	EI_MAG2		= 2, /* 'L' */
	EI_MAG3		= 3, /* 'F' */
	EI_CLASS	= 4, /* Architecture (32/64) */
	EI_DATA		= 5, /* Byte Order */
	EI_VERSION	= 6, /* ELF Version */
	EI_OSABI	= 7, /* OS Specific */
	EI_ABIVERSION	= 8, /* OS Specific */
	EI_PAD		= 9, /* Padding */
	EI_NIDENT	= 16 /* Size of e_ident[] */
};

typedef  struct
{
	Elf64_Word	sh_name;	/*  Section  name  */
	Elf64_Word	sh_type;	/*  Section  type  */
	Elf64_Xword	sh_flags;	/*  Section  attributes  */
	Elf64_Addr	sh_addr;	/*  Virtual  address  in  memory  */
	Elf64_Off	sh_offset;	/*  Offset  in  file  */
	Elf64_Xword	sh_size;	/*  Size  of  section  */
	Elf64_Word	sh_link;	/*  Link  to  other  section  */
	Elf64_Word	sh_info;	/*  Miscellaneous  information  */
	Elf64_Xword	sh_addralign;	/*  Address  alignment  boundary  */
	Elf64_Xword	sh_entsize;	/*  Size  of  entries,  if  section  has  table  */
}  Elf64_Shdr;

typedef  struct
{
	Elf64_Word	p_type;		/*  Type  of  segment  */
	Elf64_Word	p_flags;	/*  Segment  attributes  */
	Elf64_Off	p_offset;	/*  Offset  in  file  */
	Elf64_Addr	p_vaddr;	/*  Virtual  address  in  memory  */
	Elf64_Addr	p_paddr;	/*  Reserved  */
	Elf64_Xword	p_filesz;	/*  Size  of  segment  in  file  */
	Elf64_Xword	p_memsz;	/*  Size  of  segment  in  memory  */
	Elf64_Xword	p_align;	/*  Alignment  of  segment  */
}  Elf64_Phdr;

int elf_check_supported(Elf64_Ehdr *hdr) {
	if(memcmp(hdr->e_ident, "\x7F""ELF", 4)) {
		TRACE("Bad magic number");
		return 0;
	}
	if(hdr->e_ident[EI_CLASS] != 2) {
		TRACE("Bad EI_CLASS");
		return 0;
	}
	if(hdr->e_ident[EI_DATA] != 2) {
		TRACE("Bad EI_DATA");
		return 0;
	}
	if(hdr->e_ident[EI_VERSION] != 1) {
		TRACE("Bad EI_VERSION");
		return 0;
	}
	if(hdr->e_ident[EI_OSABI] != 9) {
		TRACE("Bad EI_OSABI: %X", hdr->e_ident[EI_OSABI]);
		return 0;
	}
	if(hdr->e_ident[EI_ABIVERSION] != 0) {
		TRACE("Bad EI_ABIVERSION: %X", hdr->e_ident[EI_ABIVERSION]);
		return 0;
	}
	if(hdr->e_type != 2) {
		TRACE("Bad e_type: %X", hdr->e_type);
		return 0;
	}
	if(hdr->e_machine != 8) {
		TRACE("Bad e_machine: %X", hdr->e_machine);
		return 0;
	}
	if(hdr->e_version != 1) {
		TRACE("Bad e_version");
		return 0;
	}
	if(hdr->e_flags != 0x30000007) {
		TRACE("Bad e_flags: %X", hdr->e_flags);
		return 0;
	}
	return 1;
}

#if 0
static inline Elf64_Shdr *elf_sheader(Elf64_Ehdr *hdr) {
	return (Elf64_Shdr *)((char *)hdr + hdr->e_shoff);
}
#endif

static inline Elf64_Phdr *elf_pheader(Elf64_Ehdr *hdr) {
	return (Elf64_Phdr *)((char *)hdr + hdr->e_phoff);
}

#if 0
static inline Elf64_Shdr *elf_section(Elf64_Ehdr *hdr, int idx) {
	kernel_assert(idx < hdr->e_shnum);
	return &elf_sheader(hdr)[idx];
}
#endif

static inline Elf64_Phdr *elf_segment(Elf64_Ehdr *hdr, int idx) {
	kernel_assert(idx < hdr->e_phnum);
	return &elf_pheader(hdr)[idx];
}

void tmp_exec_stuff(size_t pid, size_t base, size_t entry, size_t len);

/* not secure */
void elf_loader(const char * file) {
	int filelen=0;
	char * addr = load(file, &filelen);
	if(!addr) {
		TRACE("Could not read file");
		kernel_freeze();
		return;
	}
	Elf64_Ehdr *hdr = (Elf64_Ehdr *)addr;
	if(!elf_check_supported(hdr)) {
		TRACE("ELF File cannot be loaded");
		kernel_freeze();
		return;
	}
	TRACE("e_entry:%lX e_phnum:%d e_shnum:%d", hdr->e_entry, hdr->e_phnum, hdr->e_shnum);
	size_t allocsize = 0;
	for(int i=0; i<hdr->e_phnum; i++) {
		Elf64_Phdr *seg = elf_segment(hdr, i);
		#if 0
		TRACE("SGMT: type:%X flags:%X offset:%lX vaddr:%lX filesz:%lX memsz:%lX align:%lX",
			seg->p_type, seg->p_flags, seg->p_offset, seg->p_vaddr,
			seg->p_filesz, seg->p_memsz, seg->p_align);
		#endif
		if(seg->p_type == 1) {
			allocsize = imax(allocsize, seg->p_vaddr + seg->p_memsz);
		}
	}
	char *prgmp = kernel_calloc(allocsize, 1); //TODO:align this correctly
	bzero(prgmp, allocsize);
	if(!prgmp) {
		TRACE("Malloc failed");
		kernel_freeze();
		return;
	}
	for(int i=0; i<hdr->e_phnum; i++) {
		Elf64_Phdr *seg = elf_segment(hdr, i);
		memcpy(prgmp+seg->p_vaddr, addr + seg->p_offset, seg->p_filesz);
	}
	size_t pid = kernel_exec((uintptr_t)(prgmp + hdr->e_entry), 0x465);
	tmp_exec_stuff(pid, (size_t)prgmp, hdr->e_entry, allocsize);	
}
