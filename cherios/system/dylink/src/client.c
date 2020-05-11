/*-
 * Copyright (c) 2020 Lawrence Esswood
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

#include "cheric.h"
#include "assert.h"
#include "elf.h"
#include "crt.h"
#include "dylink.h"
#include "syscalls.h"
#include "nonce.h"
#include "msg.h"
#include "stdlib.h"
#include "temporal.h"
#include "cprogram.h"
#include "misc.h"
#include "namespace.h"
#include "macroutils.h"

// We batch symbol exchnage because otherwise there would be far too many domain crossings
#define SYMBOL_EXCHANGE_MAX 0x20

// Call one of the linking functions but in one of our partners
#define CALL_PARTNER_FUNC(p, f, ...) INVOKE_FUNCTION_POINTER((p)->info->f, data_args[(p)->ndx], __VA_ARGS__)

lib_info_t own_info;
parsed_dynamic_t own_pd;

// Standard elf hash of name appended to suffix
static unsigned long
elf_Hash2(const unsigned char* name, const unsigned char *suffix) {
    unsigned long h = 0, g;

    do {
        while (*name)
        {
            h = (h << 4) + *name++;
            if ((g = (h & 0xf0000000)))
                h ^= g >> 24;
            h &= ~g;
        }
        if(suffix == NULL) return h;
        name = suffix;
        suffix = NULL;
    } while(1);

}

// strcmp for name appended to suffix with compare
static int strcmp_2(const unsigned char* name, const unsigned char* suffix, const unsigned char* compare) {
    while((suffix != NULL && *name == '\0') || (*name == *compare++)) {
        if(*name++ == '\0') {
            if(suffix == NULL) return 0;
            name = suffix;
            suffix = NULL;
        }
    }
    return (*(const unsigned char *)name - *(const unsigned char *)(compare - 1));
}

// Bound a 0 terminated string to its length
static const unsigned char* set_str_to_len(const unsigned char* str) {
    size_t len = strlen((const char*)str);
    return (const unsigned char*)cheri_setbounds(str, len+1);
}

/*
void print_hash_table(parsed_dynamic_t* pd) {
    const Elf64_Hash* hash = pd->hash;

    syscall_puts("Hash table:\n NBuckets: ");
    char int_buf[0x10];
#define P_I(i) itoa(i, int_buf, 16); syscall_puts(int_buf)
    P_I(hash->nbucket);
    syscall_puts(", NChain: ");
    P_I(hash->nchain);
    syscall_puts("\n Buckets:\n");

    for(size_t i = 0; i != hash->nbucket; i++) {
        P_I(hash->values[i]);
        syscall_puts("\n");
    }

    syscall_puts("\n Chains:\n");

    for(size_t i = 0; i != pd->symtab_ents; i++) {
        const Elf64_Sym* sym = &pd->symtab[i];
        const unsigned char *s = pd->strtab+sym->st_name;
        P_I(i);
        syscall_puts(" | ");
        P_I(hash->values[hash->nbucket+i]);
        syscall_puts(" | ");
        syscall_puts((const char*)s);
        unsigned long h = elf_Hash2(s, NULL) % hash->nbucket;
        syscall_puts("\t\t\t\t\t");
        P_I(h);
        syscall_puts("\n");
    }
}
 */

// Return an entry in the symbol table that matches name, or NULL if none do.
// Can provide two strings, NULL suffix is treated as the empty string
static const Elf64_Sym* lookup_symbol_by_name(parsed_dynamic_t* pd, const unsigned char* name, const unsigned char* suffix) {

    const Elf64_Hash* hash = pd->hash;
    size_t nbuckets = hash->nbucket;

    unsigned long h = elf_Hash2(name, suffix);

    unsigned long symbol_ndx = hash->values[h % nbuckets];

    while(symbol_ndx != STN_UNDEF) {
        const Elf64_Sym* sym = &pd->symtab[symbol_ndx];
        if(strcmp_2(name, suffix, &pd->strtab[sym->st_name]) == 0) {
            return sym;
        }
        symbol_ndx = hash->values[nbuckets + symbol_ndx];
    }

    return NULL;
}

static capability symbol_to_capability(__unused parsed_dynamic_t* pd, const Elf64_Sym* sym) {
    if(ELF64_ST_TYPE(sym->st_info) == STT_TLS) {
        return cheri_setbounds(((char*)(&thread_local_tls_seg) + sym->st_value), sym->st_size);
    } else {
        return (capability)crt_logical_to_cap(sym->st_value,sym->st_size, ELF64_ST_TYPE(sym->st_info) == 0, NULL);
    }
}

// Parses the dynamic section.

void parse_dynamic_section(parsed_dynamic_t* pd) {

#define RELENT_SZ       16
#define PLTRELENT_SZ    16
#define PLTREL_TYPE     DT_REL
#define SYMENT_SZ       24

    size_t strtab_ptr;
    size_t rel_ptr;
    size_t jmprel_ptr;
    size_t symtab_ptr;

    size_t needed_ptrs[MAX_LIBS];

    bzero(pd, sizeof(parsed_dynamic_t));

    for(Elf64_Dyn* entry = (Elf64_Dyn*)dynamic_segment; entry->d_tag != DT_NULL; entry++) {

        const void** store_at;
        size_t ptr;
        size_t sz;

        Elf64_Hash* hash_header;

        switch(entry->d_tag) {
            case DT_NEEDED: // A pointer to shared library
                // We cant resolve strings yet and so just store the ptr
                needed_ptrs[pd->needed_ents++] = entry->d_un.d_ptr;
                continue;

            case DT_REL: // Relocations that are not PLTish (matched with RELSZ and RELENT)
                rel_ptr = entry->d_un.d_ptr;
                continue;

            case DT_RELSZ: // Size of relocations table
                store_at = (const void**)&pd->rel;
                ptr = rel_ptr;
                pd->rel_ents = entry->d_un.d_val / RELENT_SZ;
                sz =  entry->d_un.d_val;
                break;

            case DT_RELENT: // Size of relocation
                assert(entry->d_un.d_val == RELENT_SZ);
                continue;

            case DT_JMPREL: // PLT relocations (matched with PLTRELSZ and PLTREL)
                jmprel_ptr = entry->d_un.d_ptr;
                continue;

            case DT_PLTRELSZ: // Size of PLT relocations table
                store_at = (const void**)&pd->jmprel;
                ptr = jmprel_ptr;
                pd->jmprel_ents = entry->d_un.d_val / PLTRELENT_SZ;
                sz =  entry->d_un.d_val;
                break;

            case DT_PLTREL: // Type of PLT relocations
                assert(entry->d_un.d_val == PLTREL_TYPE);
                continue;

            case DT_STRTAB: // A pointer to the string table (matched with STRSZ)
                strtab_ptr = entry->d_un.d_ptr;
                continue;

            case DT_STRSZ: // Size of string table
                store_at = (const void**)&pd->strtab;
                ptr = strtab_ptr;
                sz =  entry->d_un.d_val;
                break;

            case DT_HASH:
                ptr = entry->d_un.d_val;
                // Cant know the true size until we read the header
                hash_header = (Elf64_Hash*)crt_logical_to_cap(ptr, 2 * sizeof(Elf64_Word), 0, NULL);
                pd->symtab_ents = hash_header->nchain;
                sz = (hash_header->nbucket + hash_header->nchain + 2) * sizeof(Elf64_Word);
                store_at = (const void**)&pd->hash;
                break;

            case DT_SYMTAB:
                symtab_ptr = entry->d_un.d_ptr;
                continue;

            case DT_SYMENT:
                assert(entry->d_un.d_val == SYMENT_SZ);
                continue;
            default:
                continue;
            {} // Ignore
        }

        *store_at = crt_logical_to_cap(ptr, sz, 0, NULL);
    }

    pd->symtab = (const Elf64_Sym*)crt_logical_to_cap(symtab_ptr, pd->symtab_ents * SYMENT_SZ, 0, NULL);

    assert(pd->symtab != NULL);
    assert(pd->strtab != NULL);
    assert(pd->hash != NULL);

    // At this point we should have all our tables and so can make string pointers for needed
    for(size_t i = 0; i != pd->needed_ents; i++) {
        pd->needed[i] = set_str_to_len(&pd->strtab[needed_ptrs[i]]);
    }
}

static lib_unchecked_info_t link_server_get_requirements(act_kt serv) {
    return (lib_unchecked_info_t)message_send_c(0, 0, 0, 0, NULL, NULL, NULL, NULL, serv, SYNC_CALL, DYLINK_GET_REQUIREMENTS_PORT_NO);
}

static capability link_server_new_process(act_kt serv, capability request) {
    return message_send_c(0, 0, 0, 0, request, NULL, NULL, NULL, serv, SYNC_CALL, DYLINK_NEW_PROCESS_PORT_NO);
}

#define ALLOC_REQUESTED_SIZE(X) partner->info->X ? cap_malloc(partner->info->X) : NULL

static void allocate_thread_request(link_partner_t* partner, new_thread_request_t* request) {

    request->locals_res = ALLOC_REQUESTED_SIZE(space_for_locals);
    // Stacks are allocated directly with the memory manager
    request->stack_res = mem_request(0, DEFAULT_STACK_SIZE_NO_QUEUE, EXACT_SIZE, own_mop).val;
    request->ustack_res = mem_request(0, NewTemporalStackSize, EXACT_SIZE | REPRESENTABLE | COMMIT_NOW, own_mop).val;

}

static capability send_new_process_request(link_partner_t* partner) {
    new_process_request_t request;
    // First set up request

    request.nonce = was_secure_loaded ? alloc_nonce() : NULL;
    request.client_id = was_secure_loaded ? foundation_get_id(own_auth) : NULL;
    request.globals_res = ALLOC_REQUESTED_SIZE(space_for_globals);
    request.session_res = ALLOC_REQUESTED_SIZE(space_for_session);

    allocate_thread_request(partner, &request.first_thread);

    capability req = cheri_andperm((capability)&request, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);

    res_t req_lock_cap;

    if(partner->id) {
        // Partner will expect us to lock the request
        req_lock_cap = cap_malloc(RES_CERT_META_SIZE);
        req = (capability)rescap_take_locked(req_lock_cap, NULL, 0, partner->id, NULL, req);
    } else {

    }

    capability resp = link_server_new_process(partner->server_act, req);

    assert(resp != NULL);

    if(was_secure_loaded) {
        // Result should be locked for us
        _safe cap_pair pair;
        rescap_unlock((auth_result_t)resp, &pair, own_auth, AUTH_PUBLIC_LOCKED);

        // And nonce should be matched
        assert(pair.data != NULL);
        assert(nonces_equal(pair.code,request.nonce));

        resp = pair.data;

        free_nonce(request.nonce);
    }

    if(request.session_res) free(request.session_res);

    return resp;
}

static void resolve_syms_internal(const unsigned char** in, capability* out, size_t n_syms, int functions, int internal) {

    if(internal) functions = 0;

    const unsigned char* prefix = (const unsigned char*)( was_secure_loaded ? "__cross_domain_" : "__cross_domain_trusted_");
    parsed_dynamic_t* pd = &own_pd;

    for(size_t i = 0; i != n_syms; i++) {
        const unsigned char* name = in[i];
        const Elf64_Sym* sym = lookup_symbol_by_name(pd, functions ? prefix : name, functions ? name : NULL);

        out[i] = SYM_NOT_FOUND;

        if(sym == NULL || sym->st_shndx == 0) continue;

        // We refuse to pass a reference directly to a function outside the library
        if(!functions && !internal && (ELF64_ST_TYPE(sym->st_info) == STT_FUNC)) continue;

        capability cap = symbol_to_capability(pd, sym);

        if(functions && was_secure_loaded) {
            cap = cheri_seal(cap, get_cds());
        }

        out[i] = cap;
    }
}

extern void CROSS_DOMAIN(provide_session)(link_session_t* session, res_t plt_res, int first_thread, capability* data_args);
extern void CROSS_DOMAIN(resolve_syms)(const unsigned char** in, capability* out, size_t n_syms, int functions);
extern void CROSS_DOMAIN(provide_common)(act_control_kt self_ctrl, act_kt self_ref, mop_t mop);
extern void TRUSTED_CROSS_DOMAIN(provide_session)(link_session_t* session, res_t plt_res, int first_thread, capability* data_args);
extern void TRUSTED_CROSS_DOMAIN(resolve_syms)(const unsigned char** in, capability* out, size_t n_syms, int functions);
extern void TRUSTED_CROSS_DOMAIN(provide_common)(act_control_kt self_ctrl, act_kt self_ref, mop_t mop);

void set_info_functions(lib_info_t* info) {
    if(was_secure_loaded) {
        info->provide_session = SEALED_CROSS_DOMAIN(provide_session);
        info->resolve_syms = SEALED_CROSS_DOMAIN(resolve_syms);
        info->provide_common = SEALED_CROSS_DOMAIN(provide_common);
    } else {
        info->provide_session = TRUSTED_CROSS_DOMAIN(provide_session);
        info->resolve_syms = TRUSTED_CROSS_DOMAIN(resolve_syms);
        info->provide_common = TRUSTED_CROSS_DOMAIN(provide_common);
    }
}

static void set_data_arg_0(capability* data_args) {
    CTL_t* ctl = get_ctl();
    data_args[0] = was_secure_loaded ? cheri_seal(ctl, get_cds()) : ctl;
}

// Uses the discovery service to get a handle for dynamic libraries
static void discover_libs(link_session_t* session, parsed_dynamic_t* pd, capability* data_args) {

    // The first library is the application itself
    set_info_functions(&own_info);

    session->partners[0].server_act = NULL;
    session->partners[0].info = cheri_andperm(&own_info, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
    session->partners[0].id = was_secure_loaded ? foundation_get_id(own_auth) : NULL;

    set_data_arg_0(data_args);

    for(size_t i = 0; i != pd->needed_ents; i++) {
        act_kt serv = namespace_get_ref_by_name((const char*)pd->needed[i]);

        link_partner_t* partner = &session->partners[i+1];

        if(serv == NULL) {
            printf("Failed to find library: %s\n", (const char*)pd->needed[i]);
            sleep(MS_TO_CLOCK(1000));
        }

        assert(serv != NULL);
        partner->server_act = serv;
        partner->ndx = i + 1;
        lib_unchecked_info_t uncheckedInfo = link_server_get_requirements(serv);

        assert(uncheckedInfo.as_signed != NULL);
        // TODO here is where we would check we are linking with the correct library

        if(cheri_gettype(uncheckedInfo.as_signed) == FOUND_CERT_TYPE) {
            _safe cap_pair pair;
            found_id_t* id = rescap_check_cert(uncheckedInfo.as_signed, &pair);
            partner->id = id;
            partner->info = pair.data;
        } else {
            partner->id = NULL;
            partner->info = uncheckedInfo.as_unsigned;
        }

        assert(partner->info->resolve_syms != NULL);
        assert(partner->info->provide_session != NULL);

        // Now start a session
        data_args[i+1] = send_new_process_request(partner);

        // Then make sure it can make syscalls
        CALL_PARTNER_FUNC(partner, provide_common, act_self_ctrl, act_self_ref, own_mop);
    }

    session->n_libs = pd->needed_ents + 1;
}

// These should really be generated by the static linker, but this is a temporary fix to get things working

#define F_SYM_N(n) dynamic_data_arg_ ## n
#define F_DEFINE_SYMBOL_N(n, ...) extern __thread __attribute__((weak)) VIS_HIDDEN capability F_SYM_N(n);

FOR_BASE_16(F_DEFINE_SYMBOL_N, MAX_LIBS_BASE_16)

uint16_t data_arg_ndxs[MAX_LIBS];
uint16_t mode_ndx; // For now just 1 mode, could have an array of these normally
cap_pair plt_stubs_pair;
size_t stubs_allocated;

static void init_plt_stub_allocator(res_t res) {
    _safe cap_pair pair;

    rescap_take(res, &pair);

    plt_stubs_pair = pair;

#define F_CALC_DUMMY_OFFSET(n, ...) data_arg_ndxs[n] = get_tls_sym_captable_ndx16(F_SYM_N(n));
    FOR_BASE_16(F_CALC_DUMMY_OFFSET, MAX_LIBS_BASE_16)


}

static inline size_t normalise_ndx(size_t lib_ndx, size_t own_lib_ndx) {
    return lib_ndx < own_lib_ndx ? lib_ndx : (lib_ndx - 1);
}

static void set_up_plt_dummies(__unused link_session_t* session, size_t own_lib_ndx, capability* data_args) {
    capability* captable_local = (capability*)get_ctl();

    for(size_t i = 0; i != MAX_LIBS+1; i++) {
        if(i == own_lib_ndx) continue;
        captable_local[data_arg_ndxs[normalise_ndx(i, own_lib_ndx)]] = data_args[i];
    }

    mode_ndx = (was_secure_loaded ?
                get_sym_call_captable_offset32(plt_common_untrusting) :
                get_sym_call_captable_offset32(plt_common_complete_trusting)) / sizeof(capability);

}

static capability get_current_thread_data_arg(size_t lib_ndx, size_t own_lib_ndx) {
    capability* captable_local = (capability*)get_ctl();

    return captable_local[data_arg_ndxs[normalise_ndx(lib_ndx, own_lib_ndx)]];
}

extern void plt_stub(void);

// If mode is NULL then just direct CCall
static capability make_stub(capability wrapped_symbol, size_t lib_ndx, size_t own_lib_ndx) {

    // FIXME - If this gets called twice for the same wrapped symbol it will wrap it again

    lib_ndx = normalise_ndx(lib_ndx, own_lib_ndx);

    char* code = (char*)plt_stubs_pair.code;
    char* data = (char*)plt_stubs_pair.data;

    plt_stubs_pair.code = (char*)(plt_stubs_pair.code) + PLT_STUB_SIZE;
    plt_stubs_pair.data = (char*)(plt_stubs_pair.data) + PLT_STUB_SIZE;

    // First store the wrapped symbol inline in the stub
    *((capability*)data) = wrapped_symbol;

    data += sizeof(capability);

    // Then copy the stub template
    memcpy(data, &plt_stub, PLT_STUB_CODE_SIZE);

    // Then fill in offsets

    *(uint16_t*)(data + MODE_NDX_OFF) = mode_ndx;
    *(uint16_t*)(data + DATA_NDX_OFF) = data_arg_ndxs[lib_ndx];

    return (char*)cheri_setbounds(code, PLT_STUB_SIZE) + sizeof(capability);
}

static void link_symbol(size_t offset, capability cap, int is_function, size_t lib_ndx, size_t own_lib_ndx) {

    int is_tls = (offset >> 63) != 0;
    offset = offset & ~(1ULL << 63ULL);

    capability* loc = (capability*)crt_logical_to_cap(offset, sizeof(capability), is_tls, (char*)&thread_local_tls_seg);

    // FIXME: Ignoring add-end currently
    // size_t addend = *((size_t*)loc);

    if(!is_function || (lib_ndx == own_lib_ndx)) {
        *loc = cap;
    } else {
        *loc = make_stub(cap, lib_ndx, own_lib_ndx);
    }
}

static void batch_symbols(link_session_t* session, parsed_dynamic_t* parsed, capability* data_args,
                   const unsigned char** in_blocks, capability* out_blocks,
                   size_t* offsets, size_t* block_fills, int functions, size_t own_ndx, size_t lib_ndx) {

    const unsigned char** in = cheri_setbounds(in_blocks + (lib_ndx * SYMBOL_EXCHANGE_MAX), SYMBOL_EXCHANGE_MAX* sizeof(const unsigned char*));
    in = cheri_andperm(in, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);

    capability* out = cheri_setbounds(out_blocks + (lib_ndx * SYMBOL_EXCHANGE_MAX), SYMBOL_EXCHANGE_MAX * sizeof(capability));

    size_t n_syms = block_fills[lib_ndx];

    if(lib_ndx == own_ndx) {
        resolve_syms_internal(in, out, n_syms, 0, 1);
    } else {
        CALL_PARTNER_FUNC(&session->partners[lib_ndx], resolve_syms, in, out, n_syms, functions);
    }

    size_t* offsets_lib = offsets + (lib_ndx * SYMBOL_EXCHANGE_MAX);

    for(size_t i = 0; i != n_syms; i++) {
        if(out[i] == SYM_NOT_FOUND) {
            assert(lib_ndx != (session->n_libs-1));
            size_t next_block_ndx = ((lib_ndx+1) * SYMBOL_EXCHANGE_MAX) + block_fills[lib_ndx+1]++;
            in_blocks[next_block_ndx] = in[i];
            offsets[next_block_ndx] = offsets_lib[i];
            if(block_fills[lib_ndx+1] == SYMBOL_EXCHANGE_MAX) {
                batch_symbols(session, parsed, data_args, in_blocks, out_blocks, offsets, block_fills, functions, own_ndx, lib_ndx+1);
            }
        } else {
            link_symbol(offsets_lib[i], out[i], functions, lib_ndx, own_ndx);
        }
    }

    block_fills[lib_ndx] = 0;
}

static void batch_symbols_threshold(link_session_t* session, parsed_dynamic_t* parsed, capability* data_args,
        const unsigned char** in_blocks, capability* out_blocks,
        size_t* offsets, size_t* block_fills, int functions, size_t own_ndx, size_t threshold) {

    size_t lib_ndx = 0;

    while(block_fills[lib_ndx] >= threshold) {

        if(block_fills[lib_ndx] != 0) {
            batch_symbols(session, parsed, data_args, in_blocks, out_blocks, offsets, block_fills, functions, own_ndx, lib_ndx);
        }

        if(block_fills[lib_ndx] == 0 && lib_ndx == session->n_libs-1) break;

        assert(lib_ndx != session->n_libs-1);

        lib_ndx++;
    }
}

static void handle_relocations(link_session_t* session, parsed_dynamic_t* parsed, capability* data_args,
                        const unsigned char** in_blocks, capability* out_blocks,
                        size_t* offsets, size_t* block_fills, int functions, size_t own_ndx, int global,
                        const Elf64_Rel* start, const Elf64_Rel* end) {

    for(const Elf64_Rel* rel = start; rel != end; rel++) {
        const Elf64_Sym* sym =  &parsed->symtab[ELF64_R_SYM(rel->r_info)];

        int is_tls = ELF64_ST_TYPE(sym->st_info) == STT_TLS;
        // Select which to process (0 = TLS only, 1 = global only, 2 = all)
        if(is_tls == global) continue;

        size_t block_ndx;

        switch(ELF64_R_TYPE(rel->r_info)) {
            case R_MIPS_CHERI_CAPABILITY:
            case R_MIPS_CHERI_CAPABILITY_CALL:
                block_ndx = block_fills[0]++;
                in_blocks[block_ndx] = set_str_to_len(&parsed->strtab[sym->st_name]);
                offsets[block_ndx] = rel->r_offset | (is_tls ? 1ULL << 63 : 0);
                if(block_fills[0] == SYMBOL_EXCHANGE_MAX)
                    batch_symbols_threshold(session, parsed, data_args, in_blocks, out_blocks, offsets, block_fills,
                                            functions, own_ndx, SYMBOL_EXCHANGE_MAX);
                break;
            default:
                break;
        }
    }

    batch_symbols_threshold(session, parsed, data_args, in_blocks, out_blocks, offsets, block_fills,
                            functions, own_ndx, 0);
}

static void process_own_relocations(link_session_t* session, parsed_dynamic_t* parsed, capability* data_args, int global) {
    // Currently only doing CHERI relocations

    // Shared blocks of memory for symbol exchange.

    const unsigned char* in_blocks[SYMBOL_EXCHANGE_MAX * (MAX_LIBS + 1)]; // Symbols we are looking for
    capability out_blocks[SYMBOL_EXCHANGE_MAX * (MAX_LIBS + 1)]; // Symbols returned by libs

    size_t offsets[SYMBOL_EXCHANGE_MAX * (MAX_LIBS + 1)]; // Offsets we should write results to
    size_t block_fills[MAX_LIBS + 1]; // How much is stored in each block

    bzero(in_blocks, sizeof(in_blocks));
    bzero(out_blocks,sizeof(out_blocks));
    bzero(block_fills, sizeof(block_fills));

    // We resolve symbols inside this library slightly differently
    size_t own_ndx = ~0;

    for(size_t i = 0; i != session->n_libs; i++) {
        if((size_t)data_args[i] == (size_t)get_ctl()) {
            own_ndx = i;
            break;
        }
    }

    assert(own_ndx != (size_t)~0);

    set_up_plt_dummies(session, own_ndx, data_args);

    if(parsed->rel) {
        handle_relocations(session, parsed, data_args, in_blocks, out_blocks, offsets, block_fills,
                           0, own_ndx, global, parsed->rel, parsed->rel+parsed->rel_ents);
    }

    if(parsed->jmprel && global) {

        handle_relocations(session, parsed, data_args, in_blocks, out_blocks, offsets, block_fills,
                           1, own_ndx, global, parsed->jmprel, parsed->jmprel+parsed->jmprel_ents);
    }

}

static void process_relocations_in_other_libraries(link_session_t* session, int first_thread, capability* data_args) {
    link_session_t* session_shared = cheri_andperm(session, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);

    // Now provide this session (read only) to all libraries so they can resolve their own symbols
    for(size_t i = 1; i != session->n_libs; i++) {
        link_partner_t* partner = &session->partners[i];
        CALL_PARTNER_FUNC(partner, provide_session, session_shared, cap_malloc(partner->info->space_for_plt_stubs), first_thread, data_args);
    }
}

void auto_dylink_new_process(link_session_t* session, capability* data_args) {
    if(dynamic_segment != NULL) {

        capability data_args_on_stack[MAX_LIBS+1];

        if(data_args == NULL) data_args = data_args_on_stack;

        parse_dynamic_section(&own_pd);
        discover_libs(session, &own_pd, data_args);
        if(own_pd.jmprel) init_plt_stub_allocator(cap_malloc(own_pd.jmprel_ents * PLT_STUB_SIZE));

        process_own_relocations(session, &own_pd, data_args, 2);

        process_relocations_in_other_libraries(session, 1, data_args);
    }
}

void auto_dylink_pre_new_thread(link_session_t* session, capability* data_args) {

    // This is called by the constructing thread to make space for a new thread
    // DONT use CALL_PARTNER_FUNC as we are calling into the current threads domain, not the domain of the new thread

    if(dynamic_segment != NULL) {
        new_thread_request_t request;

        for(size_t i = 1; i != session->n_libs; i++) {

            link_partner_t* p = &session->partners[i];
            allocate_thread_request(p, &request);
            data_args[i] = INVOKE_FUNCTION_POINTER(p->info->new_library_thread,
                    get_current_thread_data_arg(i, 0), &request);

        }

    }
}

void auto_dylink_post_new_thread(link_session_t* session, capability* data_args) {
    if(dynamic_segment != NULL) {

        // Main applications data arg
        set_data_arg_0(data_args);

        // Re-provide these
        for(size_t i = 1; i != session->n_libs; i++) {
            CALL_PARTNER_FUNC(&session->partners[i], provide_common, act_self_ctrl, act_self_ref, own_mop);
        }

        process_own_relocations(session, &own_pd, data_args, 0);

        process_relocations_in_other_libraries(session, 0, data_args);
    }
}

// FIXME: Do destroy thread as well. Currently memory leaks.

void resolve_syms(const unsigned char** in, capability* out, size_t n_syms, int functions) {
    return resolve_syms_internal(in, out, n_syms, functions, 0);
}

void provide_session(link_session_t* session, res_t plt_res, int first_thread, capability* data_args) {

    if(first_thread) {
        if(own_pd.jmprel) init_plt_stub_allocator(plt_res);
    }

    process_own_relocations(session, &own_pd, data_args, first_thread ? 2 : 0);
}

void provide_common(act_control_kt self_ctrl, act_kt self_ref, mop_t mop) {
    act_self_ctrl = self_ctrl;
    act_self_ref = self_ref;
    own_mop = mop;

    // Make sure we can make make syscalls
    init_kernel_if_t_new_thread(self_ctrl);
}