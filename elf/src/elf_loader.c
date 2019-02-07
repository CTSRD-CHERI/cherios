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
#include "nano/nanokernel.h"
#include "mman.h"
#include "capmalloc.h"

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
	if(hdr->e_ident[EI_ABIVERSION] > 1) {
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
#ifdef _CHERI256_
#define ELF_E_FLAGS 0x30C2C005
#else
#define ELF_E_FLAGS 0x30C1C005
#endif
	if((hdr->e_flags != 0x30000007) && (hdr->e_flags != ELF_E_FLAGS)) {
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

static inline Elf64_Phdr *elf_segment(Elf64_Ehdr *hdr, size_t idx) {
	assert(idx < hdr->e_phnum);
	return &elf_pheader(hdr)[idx];
}

/* not secure */

static void load_PT_loads(Elf_Env* env, struct image_old* elf, char* prgmp) {
    char* addr = (char*)elf->hdr;

    assert(elf != NULL);
    assert(elf->hdr != NULL);

    for(int i=0; i<elf->hdr->e_phnum; i++) {
        Elf64_Phdr *seg = elf_segment(elf->hdr, i);
        if(seg->p_type == PT_LOAD) {
            TRACE("memcpy: [%lx %lx] <-- [%lx %lx] (%lx bytes)",
                  seg->p_vaddr, seg->p_vaddr + seg->p_filesz,
                  seg->p_offset, seg->p_offset + seg->p_filesz,
                  seg->p_filesz);
            env->memcpy(prgmp+seg->p_vaddr, addr + seg->p_offset, seg->p_filesz);
        }
    }
}

static void TLS_copy(image_old* elf, size_t tls_num) {
	char* tls_base = (char*)elf->loaded_process.data + elf->tls_base + (elf->tls_size * tls_num);
	memcpy(tls_base, elf->tls_load_start, elf->tls_load_size);
	/* tbss always follows tdata. zero here */
	bzero(tls_base + elf->tls_load_size, elf->tls_size - elf->tls_load_size);
}

/* Load an image */
cap_pair create_image_old(Elf_Env *env, image_old* elf, image_old* out_elf, enum e_storage_type store_type) {

    assert(elf != NULL);
    assert(elf->hdr != NULL);

	memcpy(out_elf, elf, sizeof(image_old));

    assert(out_elf != NULL);
    assert(out_elf->hdr != NULL);

    switch(store_type) {

        case storage_process:
			out_elf->loaded_process = env->alloc(elf->maxaddr, env);
			out_elf->tls_num = 0;

			char *prgmp = out_elf->loaded_process.data;
			if(!prgmp) {
				ERROR("alloc failed");
				return NULL_PAIR;
			}

			TRACE("Allocated %lx bytes of target memory", elf->maxaddr);
			ENV_PRINT_CAP(env, prgmp);

			load_PT_loads(env, out_elf, prgmp);

			if(IS_SECURE(out_elf)) {
                assert(0);
			}

        case storage_thread:
            assert(out_elf->tls_size == 0 || out_elf->tls_num != MAX_THREADS);
			/* If secure loaded we did all TLS upfront */
            if(out_elf->tls_size != 0 && !IS_SECURE(out_elf)) {
				TLS_copy(out_elf, out_elf->tls_num);
            }
            if(out_elf->tls_size != 0) out_elf->tls_num++;
            break;
		default:
			assert(0 && "New storage not define for old image loading");
    }

	return out_elf->loaded_process;
}

static void load_seg(Elf_Env* env, size_t i, Elf64_Phdr *seg, image* out_im) {
	cap_pair seg_cap_pair = env->alloc(seg->p_memsz, env);
	capability seg_cap = (seg->p_flags & PF_X) ? seg_cap_pair.code : seg_cap_pair.data;
    if(seg->p_flags & PF_X) out_im->load_type.basic.code_write_cap = seg_cap_pair.data;
	out_im->load_type.basic.seg_table[i+1] = cheri_setbounds(seg_cap, seg->p_memsz);
	char* src = ((char*)out_im->hdr) + seg->p_offset;
	memcpy(seg_cap_pair.data, src, seg->p_filesz);
}

// TODO secure loading should put each segment directly into a (single) foundation?

int create_image(Elf_Env* env, image* in_im, image* out_im, enum e_storage_type store_type) {
	assert(in_im != NULL);
	assert(in_im->hdr != NULL);

	memcpy(out_im, in_im, sizeof(image));

	if(!IS_SECURE(out_im)) {
		switch(store_type) {
			case storage_new:
				// Needs all new segments
				for(size_t i=0; i<out_im->hdr->e_phnum; i++) {
					Elf64_Phdr *seg = elf_segment(out_im->hdr, i);
					if (seg->p_type == PT_LOAD) {
						if((seg->p_flags & PF_W) == 0) {
							load_seg(env, i, seg, out_im);
						}
					}
				}
			case storage_process:
				// Needs new writable parts
				for(size_t i=0; i<out_im->hdr->e_phnum; i++) {
					Elf64_Phdr *seg = elf_segment(out_im->hdr, i);
					if (seg->p_type == PT_LOAD) {
						if(seg->p_flags & PF_W) {
							load_seg(env, i, seg, out_im);
						}
					}
					if (seg->p_type == PT_TLS) {
						// This is the prototype
						capability data_seg = out_im->load_type.basic.seg_table[out_im->data_index];
						capability proto_tls_seg = cheri_incoffset(data_seg, out_im->tls_vaddr - out_im->data_vaddr);
						proto_tls_seg = cheri_setbounds(proto_tls_seg, seg->p_filesz);
						out_im->load_type.basic.tls_prototype = proto_tls_seg;
					}
				}
			case storage_thread:
				// Only needs new TLS
				if(out_im->tls_index != 0) {
					out_im->tls_num++;
					capability seg_tls = env->alloc(out_im->tls_mem_size, env).data;
					assert(seg_tls != NULL);
					out_im->load_type.basic.seg_table[out_im->tls_index] = seg_tls;
					// We dont memcpy from proto here as fixups may need to occur
				}
		}
	} else {
#ifndef CHERIOS_BOOT
		// For secure loaded programs what we have loaded is a prototype. Therefore we don't need a new one per process
		// Instead for each process we create a foundation

		// We allocate enough space for all segments plus a TLS segment
		size_t contig_size = out_im->image_size + out_im->tls_mem_size;
        cap_pair pair;
        size_t res_size_needed;
        res_t res_for_found;
        entry_t e0;
		switch (store_type) {
			case storage_new:
				pair = env->alloc(contig_size, env);

				out_im->load_type.secure.contig_wr = pair.data;
				out_im->load_type.secure.contig_ex = pair.code;

				for(size_t i=0; i<out_im->hdr->e_phnum; i++) {
					Elf64_Phdr *seg = elf_segment(out_im->hdr, i);
					if(seg->p_type == PT_LOAD) {
						memcpy(pair.data + seg->p_vaddr, ((char*)out_im->hdr) + seg->p_offset, seg->p_filesz);
					}
				}
			case storage_process:
                res_size_needed = FOUNDATION_META_SIZE(MAX_FOUND_ENTRIES, contig_size) + contig_size;
                res_for_found = mem_request(0, res_size_needed, NONE, env->handle).val;
                e0 = foundation_create(res_for_found, contig_size, out_im->load_type.secure.contig_wr,
						out_im->entry, MAX_FOUND_ENTRIES, 0);
				out_im->load_type.secure.foundation_res = res_for_found;
				out_im->load_type.secure.secure_entry = e0;
				assert(e0 != NULL);
			case storage_thread:
				if(out_im->tls_index != 0)
					out_im->tls_num++;
		}
#else
        assert(0);
#endif
	}
	return 0;
}

int elf_loader_mem(Elf_Env *env, Elf64_Ehdr* hdr, image* out_elf, int secure_load) {
	if(!elf_check_supported(env, hdr)) {
		ERROR("ELF File cannot be loaded");
		return -1;
	}

#ifndef ALLOW_SECURE
	assert(!secure_load && "Boot / Init cannot secure load");
#endif

	bzero(out_elf, sizeof(image));
	out_elf->hdr = hdr;

	Elf64_Addr e_entry = hdr->e_entry;
	TRACE("e_entry:%lX e_phnum:%d e_shnum:%d", hdr->e_entry, hdr->e_phnum, hdr->e_shnum);

	int has_tls = 0;
	int found_code = 0;
	int found_data = 0;

	size_t image_size = 0;

	for(size_t i=0; i<hdr->e_phnum; i++) {
		Elf64_Phdr *seg = elf_segment(hdr, i);
		TRACE("SGMT: type:%X flags:%X offset:%lX vaddr:%lX filesz:%lX memsz:%lX align:%lX",
			  seg->p_type, seg->p_flags, seg->p_offset, seg->p_vaddr,
			  seg->p_filesz, seg->p_memsz, seg->p_align);
		if(seg->p_filesz > seg->p_memsz) {
			ERROR("Section is larger in file than in memory");
			return -1;
		}
		if(seg->p_type == PT_LOAD) {

			assert((seg->p_flags & (PF_W | PF_X)) != (PF_W |PF_X));

			size_t seg_bound = seg->p_vaddr + seg->p_memsz;
			image_size = umax(image_size, seg_bound);

			if(seg->p_flags & PF_W) {
				assert(found_data == 0);
				found_data = 1;
				out_elf->data_index = (size_t)i + 1;
				out_elf->data_vaddr = seg->p_vaddr;
			} else if(seg->p_flags & PF_X) {
				assert(found_code == 0);
				found_code = 1;
				out_elf->code_index = (size_t)i + 1;
				out_elf->code_vaddr = seg->p_vaddr;
			}
		} else if(seg->p_type == PT_GNUSTACK || seg->p_type == PT_PHDR || seg->p_type == PT_GNURELRO) {
			/* Ignore these headers */
		} else if(seg->p_type == PT_TLS) {
			assert(has_tls == 0);
			has_tls = 1;
			out_elf->tls_index = (size_t)i + 1;
			out_elf->tls_vaddr = seg->p_vaddr;
			out_elf->tls_mem_size = seg->p_memsz;
			out_elf->tls_fil_size = seg->p_filesz;
		} else {
			ERROR("Unknown section");
			return -1;
		}
	}

	out_elf->entry   = e_entry;
	out_elf->image_size = image_size;
    out_elf->secure_loaded = secure_load;

	return create_image(env, out_elf, out_elf, storage_new);
}

/* Parse an elf file already resident in memory. */
cap_pair elf_loader_mem_old(Elf_Env *env, void *p, image_old* out_elf, int secure_load) {
	char *addr = (char *)p;
	size_t lowaddr = (size_t)(-1);
	Elf64_Ehdr *hdr = (Elf64_Ehdr *)addr;
	if(!elf_check_supported(env, hdr)) {
		ERROR("ELF File cannot be loaded");
		return NULL_PAIR;
	}

	assert(!secure_load && "Use the new loader function if you want secure loading");

    bzero(out_elf, sizeof(image_old));
    out_elf->hdr = hdr;

	Elf64_Addr e_entry = hdr->e_entry;
	TRACE("e_entry:%lX e_phnum:%d e_shnum:%d", hdr->e_entry, hdr->e_phnum, hdr->e_shnum);

	size_t allocsize = 0;
    size_t tls_align = 0;

    int has_tls = 0;

	for(int i=0; i<hdr->e_phnum; i++) {
		Elf64_Phdr *seg = elf_segment(hdr, i);
		TRACE("SGMT: type:%X flags:%X offset:%lX vaddr:%lX filesz:%lX memsz:%lX align:%lX",
			  seg->p_type, seg->p_flags, seg->p_offset, seg->p_vaddr,
			  seg->p_filesz, seg->p_memsz, seg->p_align);
		if(seg->p_filesz > seg->p_memsz) {
			ERROR("Section is larger in file than in memory");
			return NULL_PAIR;
		}
		if(seg->p_type == PT_LOAD) {
			size_t bound = seg->p_vaddr + seg->p_memsz;
			allocsize = umax(allocsize, bound);
			lowaddr = umin(lowaddr, seg->p_vaddr);
			TRACE("lowaddr:%lx allocsize:%lx bound:%lx", lowaddr, allocsize, bound);
		} else if(seg->p_type == PT_GNUSTACK || seg->p_type == PT_PHDR || seg->p_type == PT_GNURELRO) {
            /* Ignore these headers */
        } else if(seg->p_type == PT_TLS) {
            assert(has_tls == 0);
            has_tls = 1;
            out_elf->tls_load_start = addr + seg->p_offset;
            out_elf->tls_load_size = seg->p_filesz;
            out_elf->tls_size = seg->p_memsz;
            tls_align = seg->p_align;
		} else {
			ERROR("Unknown section");
			return NULL_PAIR;
		}
	}

    if(has_tls) {
        /* Round allocsize to tls_align */
        allocsize = align_up_to(allocsize, tls_align);
		out_elf->tls_base = allocsize;
		allocsize += (MAX_THREADS)*out_elf->tls_size;
    }

	out_elf->minaddr = lowaddr;
	out_elf->maxaddr = allocsize;
	out_elf->entry   = e_entry;

	out_elf->secure_loaded = secure_load;

	return create_image_old(env, out_elf, out_elf, storage_process);
}