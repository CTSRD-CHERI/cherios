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

#ifdef CHERIOS_BOOT

#include <elf.h>
#include "boot/boot.h"

#define assert(e) boot_assert(e)

#else  /* CHERIOS_BOOT */

#include "assert.h"

#endif /* CHERIOS_BOOT */

#include "colors.h"
#include "cheric.h"
#include "math.h"
#include "string.h"
#include "elf.h"

#define TRACE_ELF_LOADER	0

#if TRACE_ELF_LOADER

#define TRACE(s, ...) trace_elf_loader(env, KYLW"elf_loader: " s KRST"\n", __VA_ARGS__)
static void trace_elf_loader(Elf_Env *env, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	env->vprintf(fmt, ap);
	va_end(ap);
}

#define ENV_PRINT_CAP(env, cap)						\
	env->printf("%-20s: %-16s t:%lx s:%lx p:%08jx "			\
	       "b:%016jx l:%016zx o:%jx\n",				\
	   __func__,							\
	   #cap,							\
	   cheri_gettag(cap),						\
	   cheri_getsealed(cap),					\
	   cheri_getperm(cap),						\
	   cheri_getbase(cap),						\
	   cheri_getlen(cap),						\
	   cheri_getoffset(cap))

#else

#define TRACE(...)
#define ENV_PRINT_CAP(...)

#endif

#define ERROR(s) error_elf_loader(env, KRED"elf_loader: " s KRST"\n")
#define ERRORM(s, ...) error_elf_loader(env, KRED"elf_loader: " s KRST"\n", __VA_ARGS__)
static void error_elf_loader(Elf_Env *env, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	env->printf("%s", KRED"elf_loader: ");
	env->vprintf(fmt, ap);
	env->printf("%s", KRST"\n");
	va_end(ap);
}

int elf_check_supported(Elf_Env *env, Elf64_Ehdr *hdr) {
	if (hdr->e_ident[0] != 0x7f ||
		hdr->e_ident[1] != 'E'  ||
		hdr->e_ident[2] != 'L'  ||
		hdr->e_ident[3] != 'F') {
		ERROR("Bad magic number");
		return 0;
	}
	if(hdr->e_ident[EI_CLASS] != 2) {
		ERROR("Bad EI_CLASS");
		return 0;
	}
	if(hdr->e_ident[EI_DATA] != 2) {
		ERROR("Bad EI_DATA");
		return 0;
	}
	if(hdr->e_ident[EI_VERSION] != 1) {
		ERROR("Bad EI_VERSION");
		return 0;
	}
	if(hdr->e_ident[EI_OSABI] != 9) {
		ERRORM("Bad EI_OSABI: %X", hdr->e_ident[EI_OSABI]);
		return 0;
	}
	if(hdr->e_ident[EI_ABIVERSION] != 0) {
		ERRORM("Bad EI_ABIVERSION: %X", hdr->e_ident[EI_ABIVERSION]);
		return 0;
	}
	if(hdr->e_type != 2) {
		ERRORM("Bad e_type: %X", hdr->e_type);
		return 0;
	}
	if(hdr->e_machine != 8) {
		ERRORM("Bad e_machine: %X", hdr->e_machine);
		return 0;
	}
	if(hdr->e_version != 1) {
		ERROR("Bad e_version");
		return 0;
	}
	if((hdr->e_flags != 0x30000007) && (hdr->e_flags != 0x30C2C005)) {
		ERRORM("Bad e_flags: %X", hdr->e_flags);
		return 0;
	}
	return 1;
}

#if 0
static inline Elf64_Shdr *elf_sheader(Elf64_Ehdr *hdr) {
	return (Elf64_Shdr *)((char *)hdr + hdr->e_shoff);
}
static inline Elf64_Shdr *elf_section(Elf64_Ehdr *hdr, int idx) {
	assert(idx < hdr->e_shnum);
	return &elf_sheader(hdr)[idx];
}
#endif

static inline Elf64_Phdr *elf_pheader(Elf64_Ehdr *hdr) {
	return (Elf64_Phdr *)((char *)hdr + hdr->e_phoff);
}

static inline Elf64_Phdr *elf_segment(Elf64_Ehdr *hdr, int idx) {
	assert(idx < hdr->e_phnum);
	return &elf_pheader(hdr)[idx];
}

/* not secure */

void *elf_loader_mem(Elf_Env *env, void *p, size_t *minaddr, size_t *maxaddr, size_t *entry) {
	char *addr = (char *)p;
	size_t lowaddr = (size_t)(-1);
	Elf64_Ehdr *hdr = (Elf64_Ehdr *)addr;
	if(!elf_check_supported(env, hdr)) {
		ERROR("ELF File cannot be loaded");
		return NULL;
	}

	Elf64_Addr e_entry = hdr->e_entry;
	TRACE("e_entry:%lX e_phnum:%d e_shnum:%d", hdr->e_entry, hdr->e_phnum, hdr->e_shnum);

	size_t allocsize = 0;
	for(int i=0; i<hdr->e_phnum; i++) {
		Elf64_Phdr *seg = elf_segment(hdr, i);
		TRACE("SGMT: type:%X flags:%X offset:%lX vaddr:%lX filesz:%lX memsz:%lX align:%lX",
			  seg->p_type, seg->p_flags, seg->p_offset, seg->p_vaddr,
			  seg->p_filesz, seg->p_memsz, seg->p_align);
		if(seg->p_filesz > seg->p_memsz) {
			ERROR("Section is larger in file than in memory");
			return NULL;
		}
		if(seg->p_type == PT_LOAD) {
			size_t bound = seg->p_vaddr + seg->p_memsz;
			allocsize = umax(allocsize, bound);
			lowaddr = umin(lowaddr, seg->p_vaddr);
			TRACE("lowaddr:%lx allocsize:%lx bound:%lx", lowaddr, allocsize, bound);
		} else if(seg->p_type == PT_GNUSTACK || seg->p_type == PT_PHDR || seg->p_type == PT_GNURELRO) {
            /* Ignore these headers */
		} else {
			ERROR("Unknown section");
			return NULL;
		}
	}

	char *prgmp = env->alloc(allocsize);
	if(!prgmp) {
		ERROR("alloc failed");
		return NULL;
	}

	TRACE("Allocated %lx bytes of target memory", allocsize);
	ENV_PRINT_CAP(env, prgmp);

	for(int i=0; i<hdr->e_phnum; i++) {
		Elf64_Phdr *seg = elf_segment(hdr, i);
		if(seg->p_type == 1) {
			TRACE("memcpy: [%lx %lx] <-- [%lx %lx] (%lx bytes)",
				  seg->p_vaddr, seg->p_vaddr + seg->p_filesz,
				  seg->p_offset, seg->p_offset + seg->p_filesz,
				  seg->p_filesz);
			env->memcpy(prgmp+seg->p_vaddr, addr + seg->p_offset, seg->p_filesz);
		}
	}
	env->free(addr);

	if(minaddr)	*minaddr = lowaddr;
	if(maxaddr)	*maxaddr = allocsize;
	if(entry)	*entry   = e_entry;

	return prgmp;
}