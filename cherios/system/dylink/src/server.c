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

#include "stdio.h"
#include "assert.h"
#include "cprogram.h"
#include "cdefs.h"
#include "crt.h"
#include "temporal.h"
#include "misc.h"
#include "namespace.h"

// If 1 then we treat the program image as a prototype for copying each time a new process starts
// Otherwise the library belongs to the first process which wishes to claim it and we use the program image directly

#define MULTI_PROCESS_LINK 0

cert_t signed_info;

// roughly a page of reservation memory needed for signing and such
res_t early_alloc_block;
size_t early_alloc_size;

static res_t get_early_res(size_t size) {
    if(early_alloc_size < size || early_alloc_block == NULL) {
        early_alloc_block = mem_request(0, MEM_REQUEST_MIN_REQUEST, NONE, own_mop).val;
        early_alloc_size = MEM_REQUEST_MIN_REQUEST - RES_META_SIZE;
    }
    res_t result = early_alloc_block;
    early_alloc_block = rescap_split(early_alloc_block, size);
    early_alloc_size -= size + RES_META_SIZE;
    return result;
}

// Called via message send

lib_unchecked_info_t get_requirements(void) {
    if(!was_secure_loaded) {
        return (lib_unchecked_info_t){.as_unsigned = cheri_andperm(&own_info, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP)};
    } else {
        return (lib_unchecked_info_t){.as_signed = signed_info};
    }
}

static capability new_thread_internal(new_thread_request_t* thread_request, capability cds, capability cgp) {
    _safe cap_pair pair;

    rescap_take(thread_request->locals_res, &pair);

    if(pair.data == NULL || cheri_getlen(pair.data) < own_info.space_for_locals) return NULL;

    capability tls_seg = pair.data;

    assert(tls_seg != NULL);

    rescap_take(thread_request->stack_res, &pair);
    capability stack = pair.data;
    assert(stack != NULL);
    stack = cheri_setoffset(stack, cheri_getlen(stack));

    rescap_take(thread_request->ustack_res, &pair);
    capability ustack = pair.data;
    assert(ustack != NULL);
    ustack = cheri_setoffset(ustack, cheri_getlen(ustack));

    // Create a new segment table and insert our new tls segment

    capability seg_table[MAX_SEGS];


    memcpy(seg_table, crt_segment_table, sizeof(seg_table));
    seg_table[crt_tls_seg_off/sizeof(capability)] = tls_seg;
    memcpy(tls_seg, crt_tls_proto, crt_tls_proto_size);

    crt_init_new_locals(seg_table, RELOCS_START, RELOCS_END);

    CTL_t* locals = (CTL_t*)((char*)tls_seg + crt_cap_tab_local_addr);

    // This is being run in the _constructors_ thread, so we need to set this via offset to the new locals
    // Otherwise we would target our own copy
    size_t ndx = get_tls_sym_captable_ndx16(thread_local_tls_seg);
    ((capability*)locals)[ndx] = tls_seg;

    locals->cds = cds;
    locals->cgp = cgp;
    locals->cdl = &entry_stub;
    locals->csp = stack;
    locals->cusp = ustack;
    locals->guard.guard = callable_ready;

    capability sealed_locals = was_secure_loaded ? cheri_seal(locals, locals->cds) : locals;

    return sealed_locals;
}

capability new_library_thread(new_thread_request_t* thread_request) {
    return new_thread_internal(thread_request, get_ctl()->cds, get_ctl()->cgp);
}

static capability new_process_internal(new_process_request_t* process_request) {
    // FIXME: If there are multiple users this is not safe enough
    _Static_assert(MULTI_PROCESS_LINK == 0, "TODO");

#if (MULTI_PROCESS_LINK == 0)
    // FIXME: Make atomic
    static int process_already_linked = 0;

    if(process_already_linked) return NULL;

    process_already_linked = 1;
#endif

    capability sealed_locals = new_thread_internal(&process_request->first_thread, get_ctl()->cds, get_ctl()->cgp);

    found_id_t* client_id = process_request->client_id;

    if(client_id != NULL) {
        return (capability)rescap_take_locked(process_request->session_res, NULL, 0,
                client_id, process_request->nonce, sealed_locals);
    } else {
        return sealed_locals;
    }

}

// Called by message send

capability new_process(capability unchecked_request) {

    // Unchecked request should be locked if this library is secure loaded
    new_process_request_t* request;

    if(was_secure_loaded) {
        _safe cap_pair pair;
        rescap_unlock((auth_result_t)unchecked_request, &pair, own_auth, AUTH_PUBLIC_LOCKED);
        if(pair.data == NULL) return NULL;
        request = (new_process_request_t*)pair.data;
    } else {
        request = (new_process_request_t*)unchecked_request;
    }

    return new_process_internal(request);
}

void (*msg_methods[]) = {&get_requirements, &new_process};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

capability CROSS_DOMAIN(new_library_thread)(new_thread_request_t* thread_request);
capability TRUSTED_CROSS_DOMAIN(new_library_thread)(new_thread_request_t* thread_request);

int main(__unused register_t arg, capability carg) {

    // Parse dynamic
    parse_dynamic_section(&own_pd);

    // Will need space for globals
_Static_assert(MULTI_PROCESS_LINK == 0, "TODO");

#if (MULTI_PROCESS_LINK == 0)
    own_info.space_for_globals = 0;
#endif

    own_info.space_for_locals = crt_tls_seg_size;

    // Session + space to lock session for caller if need be
    own_info.space_for_session = RES_CERT_META_SIZE;

    own_info.space_for_plt_stubs = (own_pd.jmprel_ents * PLT_STUB_SIZE);
    // Sign + seal interface if needed

    if(was_secure_loaded) {
        res_t res = get_early_res(RES_CERT_META_SIZE);
        signed_info = rescap_take_authed(res, NULL, 0,
                AUTH_CERT, own_auth, NULL, cheri_andperm(&own_info,CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP)).cert;
    }

    set_info_functions(&own_info);

    if(was_secure_loaded) {
        own_info.new_library_thread = SEALED_CROSS_DOMAIN(new_library_thread);
    } else {
        own_info.new_library_thread = TRUSTED_CROSS_DOMAIN(new_library_thread);
    }

    // Register lib

    const char* name = (const char*)carg;

    int res = namespace_register_name(name, act_self_ref);

    assert(res == 0);

    // Go into daemon mode
    msg_enable = 1;

    return 0;
}