/*-
 * Copyright (c) 2018 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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
#ifndef CHERIOS_CRT_H
#define CHERIOS_CRT_H

#include "dylink.h"
#include "elf.h"
#include "math.h"
#include "string.h"

enum capreloc_flags_e {
    RELOC_FLAGS_NONE = 0,
    RELOC_FLAGS_TLS = 1,
    RELOC_FLAGS_NO_OUTPUT = 2
};

struct capreloc
{
    uint64_t capability_location;
    uint64_t object;
    uint64_t offset;
    uint64_t size;
    uint32_t permissions;
    enum capreloc_flags_e flags : 16;
    uint8_t location_seg_ndx;
    uint8_t object_seg_ndx;
};

#define RELOC_GET_LOC(tbl, rlc) ((capability*)(tbl[(uint8_t)(rlc->location_seg_ndx)] + rlc->capability_location))

extern void __cap_table_start;
extern void __cap_table_local_start;

__attribute__((weak))
extern struct capreloc __start___cap_relocs;
__attribute__((weak))
extern struct capreloc __stop___cap_relocs; // Stops making sense after compaction. Use size instead.

#define RELOCS_START    (&__start___cap_relocs)
#define RELOCS_END      (cheri_incoffset(&__start___cap_relocs, cap_relocs_size))

extern size_t cap_relocs_size;
extern capability crt_segment_table[MAX_SEGS];
extern size_t crt_code_seg_offset;
extern size_t crt_tls_seg_size;

extern size_t crt_cap_tab_local_addr;
extern capability crt_tls_proto;
extern size_t crt_tls_proto_size;
extern size_t crt_tls_seg_off; // _offset_ (not index)

// Need inlining as calling functions requires globals

static inline capability __attribute__((always_inline)) crt_init_common(capability* segment_table, struct capreloc* start, struct capreloc* end, int globals) {
    for(struct capreloc* reloc = start; reloc != end; reloc++) {
        uint8_t loc_seg_ndx = (uint8_t)(reloc->location_seg_ndx);
        uint8_t ob_seg_ndx = (uint8_t)(reloc->object_seg_ndx);

        // Skip depending on whether we are processing globals or not
        if((reloc->flags & RELOC_FLAGS_TLS) == globals) continue;

        capability loc_cap = segment_table[loc_seg_ndx] + reloc->capability_location;
        capability ob_cap = segment_table[ob_seg_ndx] + reloc->object;

        size_t size = reloc->size;
        size_t offset = reloc->offset;

        if ((size != 0))
        {
            //ob_cap = cheri_setbounds_exact(ob_cap, reloc->size);
            /*
            // FIXME Dedup needs alignment to 8 bytes
            // FIXME Dirty hack to align data in the execute section. This should be done by the compiler / linker.
            if((cheri_getperm(ob_cap) & CHERI_PERM_EXECUTE)) {
                size_t mis_align = (size_t)ob_cap & 0x7;
                size += mis_align;
                ob_cap= (capability)(((char*)ob_cap) - mis_align);
                offset +=mis_align;
                size = (size + 7) & ~7;
            }
            */
            ob_cap = cheri_setbounds(ob_cap, size); // Non exact until the linker generates good globals
        } else {
            ob_cap = cheri_setoffset(NULL, (size_t)ob_cap); // Being mean. If you don't give a size you don't get anything.
        }

        ob_cap = cheri_incoffset(ob_cap, offset);

        // TODO we should pay attention to the permission bits. But for now we ignore them.

        *(capability*)loc_cap = ob_cap;
    }

    if(globals == 0) {
        __asm __volatile (
        "cscbi  %[cgp], (%[i])($idc)\n"
        :
        :[i]"i"(CTLP_OFFSET_CGP), [cgp]"C"(&__cap_table_start)
        :
        );
    } else {
        cap_relocs_size = (size_t)end - (size_t)start;
        memcpy(crt_segment_table, segment_table, umin(sizeof(crt_segment_table), cheri_getlen(segment_table)));
    }
}

void __attribute__((always_inline)) crt_init_new_globals(capability* segment_table, struct capreloc* start, struct capreloc* end);

void  crt_init_new_locals(capability* segment_table, struct capreloc* start, struct capreloc* end);
void __attribute__((always_inline)) crt_init_new_locals_inline(capability* segment_table, struct capreloc* start, struct capreloc* end);
#endif //CHERIOS_CRT_H
