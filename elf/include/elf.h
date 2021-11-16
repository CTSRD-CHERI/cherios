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

#ifndef __ELF_H
#define __ELF_H

#include "elf_platform.h"

#ifndef CHERIOS_BOOT
	#define ALLOW_SECURE
#endif

#ifdef ALLOW_SECURE
#define IS_SECURE(im) ((im)->secure_loaded)
#else
#define IS_SECURE(im) 0
#endif

#define EHDR_OFF_e_phnum 56
#define EHDR_OFF_e_phoff 32

#define PHDR_SIZE 56
#define PHDR_OFF_p_type 0
#define PHDR_OFF_p_flags 4
#define PHDR_OFF_p_offset 8
#define PHDR_OFF_p_vaddr 16
#define PHDR_OFF_p_paddr 24
#define PHDR_OFF_p_filesz 32
#define PHDR_OFF_p_memsz 40
#define PHDR_OFF_p_align 48

#define MAX_SEGS 8

#ifndef __ASSEMBLY__

#include "stddef.h"
#include "stdarg.h"
#include "nano/nanotypes.h"
#include "types.h"

typedef uint16_t Elf64_Half;	// Unsigned half int
typedef uint64_t Elf64_Off;	// Unsigned offset
typedef uint64_t Elf64_Addr;	// Unsigned address
typedef uint32_t Elf64_Word;	// Unsigned int
typedef int32_t  Elf64_Sword;	// Signed int
typedef uint64_t Elf64_Xword;	// Unsigned int
typedef int64_t  Elf64_Sxword;	// Signed int

typedef struct {
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

#define CHK_EHDR(X) _Static_assert(__CONCAT(EHDR_OFF_, X) == __offsetof(Elf64_Ehdr, X), #X " offset macro is wrong")

CHK_EHDR(e_phnum);
CHK_EHDR(e_phoff);

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

typedef struct {
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

typedef struct {
	Elf64_Word	p_type;		/*  Type  of  segment  */
	Elf64_Word	p_flags;	/*  Segment  attributes  */
	Elf64_Off	p_offset;	/*  Offset  in  file  */
	Elf64_Addr	p_vaddr;	/*  Virtual  address  in  memory  */
	Elf64_Addr	p_paddr;	/*  Reserved  */
	Elf64_Xword	p_filesz;	/*  Size  of  segment  in  file  */
	Elf64_Xword	p_memsz;	/*  Size  of  segment  in  memory  */
	Elf64_Xword	p_align;	/*  Alignment  of  segment  */
}  Elf64_Phdr;

typedef struct {
    Elf64_Xword d_tag;
    union {
        Elf64_Xword     d_val;
        Elf64_Addr      d_ptr;
    } d_un;
} Elf64_Dyn;

typedef struct {
    Elf64_Word nbucket;
    Elf64_Word nchain;
    Elf64_Word values[];
} Elf64_Hash;

typedef struct {
    Elf64_Word nbucket;
    Elf64_Word nchain;
    Elf64_Word values[];
} Elf32_Hash;

typedef struct {
    Elf64_Word      st_name;
    unsigned char   st_info;
    unsigned char   st_other;
    Elf64_Half      st_shndx;
    Elf64_Addr      st_value;
    Elf64_Xword     st_size;
} Elf64_Sym;

typedef struct {
    Elf64_Addr      r_offset;
    Elf64_Xword     r_info;
} Elf64_Rel;

typedef struct {
    Elf64_Addr      r_offset;
    Elf64_Xword     r_info;
    Elf64_Sxword    r_addend;
} Elf64_Rela;

enum reloc_type
{
  R_MIPS_NONE = 0,
  R_MIPS_16,		R_MIPS_32,
  R_MIPS_REL32,		R_MIPS_26,
  R_MIPS_HI16,		R_MIPS_LO16,
  R_MIPS_GPREL16,	R_MIPS_LITERAL,
  R_MIPS_GOT16,		R_MIPS_PC16,
  R_MIPS_CALL16,	R_MIPS_GPREL32,
  R_MIPS_CHERI_CAPABILITY = 0x5A,
  R_MIPS_CHERI_CAPABILITY_CALL
};

_Static_assert(sizeof(Elf64_Phdr) == PHDR_SIZE, "Wrong phdr size");
#define CHK_PHDR(X) _Static_assert(__CONCAT(PHDR_OFF_, X) == __offsetof(Elf64_Phdr, X), #X " offset macro is wrong")
CHK_PHDR(p_type);
CHK_PHDR(p_flags);
CHK_PHDR(p_offset);
CHK_PHDR(p_vaddr);
CHK_PHDR(p_paddr);
CHK_PHDR(p_filesz);
CHK_PHDR(p_memsz);
CHK_PHDR(p_align);


/* Calling environment for loader */
typedef struct Elf_Env {
	cap_pair (*alloc)(size_t size, struct Elf_Env * handle);
	void (*free)(void *addr, struct Elf_Env* handle);
	int (*printf)(const char *fmt, ...);
	int (*vprintf)(const char *fmt, va_list ap);
	void *(*memcpy)(void *dest, const void *src, size_t n);
	capability handle;
} Elf_Env;

enum e_section_uniqueness {
	per_library,
	per_process,
	per_thread
};

#define section_table_hash(X, T) (((X) >> (T)->hash_shift) % (T)->hash_mod)
#define SHT_ENTRY(X, T) (((T)->table)[section_table_hash((X),(T))])
#define SYMBOL_CAP(X, T) (SHT_ENTRY(X, T).section_cap + (X - SHT_ENTRY(X,T).image_offset))

typedef struct image_old {
	/* Pointer to file for when we need the headers again*/
	Elf64_Ehdr *hdr;

	/* These are per process. Note if secure loaded, this cap_pair is a COPY of the real image */
	cap_pair loaded_process;
	size_t minaddr, maxaddr, entry;

	/* TLS stuff */
	char* tls_load_start;
	size_t tls_load_size;

	size_t tls_base;
	size_t tls_size;
	size_t tls_num;

	int secure_loaded;

	/* Only set when secure loaded */
	/* This entry will transfer the copied foundation + entry */
	entry_t secure_entry;
	/* This reservation was taken to create the foundation. It can be revoked here if need be */
	res_t foundation_res;
} image_old;

typedef struct image {
	/* Pointer to file for when we need the headers again*/
    const Elf64_Ehdr *hdr;

	size_t tls_index;	// 0 if none
	size_t data_index;
	size_t code_index;

#define ELF_IMAGE_VADDR(Im, Seg) (Im->load_type.basic.tables.seg_table_vaddrs[Im-> Seg ## _index])

    size_t dynamic_vaddr; // 0 if none
    size_t dynamic_size;

	size_t image_size;

	size_t tls_mem_size;
	size_t tls_fil_size;
	size_t tls_num;

	size_t entry;

	int secure_loaded;

	union {
		struct {
			char* contig_wr;
			char* contig_ex;
			entry_t secure_entry;
			res_t foundation_res;
		} secure;
		struct {
		    struct {
                capability seg_table[MAX_SEGS];
                size_t seg_table_vaddrs[MAX_SEGS];
		    } tables;
			capability tls_prototype; // The prototype for tls - actually a part of data
			capability code_write_cap; // Needed for PLT stubs. I don't like having this, we should move the stub data
		} basic;
	} load_type;

} image;

/* Currently the only supported models */
enum e_storage_type {
	storage_new,
	storage_process,
	storage_thread,
};

static inline capability make_global_pcc(image* im) {

    if(im->secure_loaded) return NULL;

	capability pcc = im->load_type.basic.tables.seg_table[im->code_index];
	pcc = cheri_setoffset(pcc, im->entry - ELF_IMAGE_VADDR(im, code));
	pcc = cheri_andperm(pcc, (CHERI_PERM_GLOBAL | CHERI_PERM_EXECUTE | CHERI_PERM_LOAD
							  | CHERI_PERM_LOAD_CAP | CHERI_PERM_CCALL));
	return pcc;
}

/* given pointer p to ELF image, returns a pointer to the loaded
   image.  if provided, it also sets the min and max addresses touched
   by the loader, and the entry point.
 */

cap_pair elf_loader_mem_old(Elf_Env *env, void *p, image_old* out_elf, int secure_load);

/* Given a pointer to loaded image creates another image that shares storage with the first image
 * specified by image type */

cap_pair create_image_old(Elf_Env *env, image_old* elf, image_old* out_elf, enum e_storage_type store_type);

int create_image(Elf_Env* env, image* in_im, image* out_im, enum e_storage_type store_type);
int elf_loader_mem(Elf_Env *env, const Elf64_Ehdr* hdr, image* out_elf, int secure_load);


#endif // Assembly

#define MAX_THREADS_FOR_OLD 1 // We no longer over allocate TLS by this much, but this number is still used for boot
					  // and for secure loading
#define MAX_FOUND_ENTRIES (4) // Only really use one now

#define PT_NULL 	0
#define PT_LOAD 	1
#define PT_DYNAMIC 	2
#define PT_INTERP 	3
#define PT_NOTE 	4
#define PT_SHLIB 	5
#define PT_PHDR 	6
#define PT_TLS 		7
#define PT_LOOS 	0x60000000
#define PT_HIOS 	0x6fffffff
#define PT_LOPROC 	0x70000000
#define PT_HIPROC 	0x7fffffff
#define PT_GNURELRO 0x6474E552
#define PT_GNUSTACK 0x6474E551
#define PT_GNU_EH_FRAME 0x6474e550

#define PT_MIPS_REGINFO		0x70000000
#define PT_MIPS_RTPROC		0x70000001
#define PT_MIPS_OPTIONS		0x70000002
#define PT_MIPS_ABI_FLAGS   0x70000003

#define PF_NONE		0
#define PF_X		1
#define PF_W		2
#define PF_R		4

// Dynamic section entry types

#define DT_NULL     0x0
#define DT_NEEDED   0x1

#define DT_REL      0x11
#define DT_RELSZ    0x12
#define DT_RELENT   0x13

#define DT_JMPREL   0x17
#define DT_PLTRELSZ 0x2
#define DT_PLTREL   0x14

#define DT_STRTAB   0x5
#define DT_STRSZ    0xa

#define DT_SYMTAB   0x6
#define DT_SYMENT   0xb

#define DT_HASH     0x4

#define MIPS_CHERI___CAPRELOC       0x000000007000c000
#define MIPS_CHERI___CAPRELOCSSZ    0x000000007000c001

// Symbols table binding types

#define STB_LOCAL 	0
#define STB_GLOBAL 	1
#define STB_WEAK 	2
#define STB_LOOS 	10
#define STB_HIOS 	12
#define STB_LOPROC 	13
#define STB_HIPROC 	15

// Symbol table symbol types

#define STT_NOTYPE 	0
#define STT_OBJECT 	1
#define STT_FUNC 	2
#define STT_SECTION 3
#define STT_FILE 	4
#define STT_COMMON 	5
#define STT_TLS 	6
#define STT_LOOS 	10
#define STT_HIOS 	12
#define STT_LOPROC 	13
#define STT_HIPROC 	15

#define ELF64_ST_BIND(info)          ((info) >> 4)
#define ELF64_ST_TYPE(info)          ((info) & 0xf)
#define ELF64_ST_INFO(bind, type)    (((bind)<<4)+((type)&0xf))

#define ELF32_ST_VISIBILITY(o)       ((o)&0x3)
#define ELF64_ST_VISIBILITY(o)       ((o)&0x3)

#define STN_UNDEF 0

#define ELF64_R_SYM(info)             ((info)>>32)
#define ELF64_R_TYPE(info)            ((Elf64_Word)(info))
#define ELF64_R_INFO(sym, type)       (((Elf64_Xword)(sym)<<32)+ \
                                        (Elf64_Xword)(type))

#endif
