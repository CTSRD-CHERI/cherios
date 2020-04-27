/*-
 * Copyright (c) 2019 Lawrence Esswood
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

#include <sys/mman.h>
#include <nano/nanokernel.h>
#include <crt.h>
#include "cheric.h"
#include "namespace.h"
#include "object.h"
#include "assert.h"
#include "dylink.h"
#include "cheriplt.h"
#include "nano/usernano.h"
#include "crt.h"
#include "misc.h"

// DEPRACATED: This is for the old manual dynamic linking.

PLT_ty(LIB_IF_T, LIB_IF_LIST)
PLT_define(LIB_IF_LIST)

#define DECLARE_CROSS_DOMAIN_SYMBOL(name, ret, sig, ...) extern ret CROSS_DOMAIN(name) sig;
LIB_IF_LIST(DECLARE_CROSS_DOMAIN_SYMBOL)


cert_t if_cert;

// We still don't have dynamic linking, so this is yet another hacked up dynamic library
// This is how it should look in the general case (the kernel and nano kernel are somewhat special)
// Users call a method for how much space is needed, get a certified procedure table and sealed idc
// They install these, and then call init_external_thread which will do object init
// These are methods of the link server and called via message pass

// Because we don't have a dynamic linker, we will just duplicate all the locals symbols rather than have them be unique
// This is fine for the purposes of this library.
// In the general case symbols would have to be exchanged both ways (in a signed / locked block)

size_t get_size_for_new(void) {
    return crt_tls_seg_size;
}

cert_t get_if_cert(void) {
    return if_cert;
}

// Should copy data segment. Ive just moved some globals this library uses into locals.

single_use_cert create_new_external_thread(res_t locals_res, res_t stack_res, res_t ustack_res, res_t sign_res) {
    _safe cap_pair pair;

    rescap_take(locals_res, &pair);

    if(pair.data == NULL || cheri_getlen(pair.data) < get_size_for_new()) return NULL;

    capability tls_seg = pair.data;

    rescap_take(stack_res, &pair);
    capability stack = pair.data;
    stack = cheri_setoffset(stack, cheri_getlen(stack));

    rescap_take(ustack_res, &pair);
    capability ustack = pair.data;
    ustack = cheri_setoffset(ustack, cheri_getlen(ustack));

    // Create a new segment table and insert our new tls segment

    capability seg_table[MAX_SEGS];

    memcpy(seg_table, crt_segment_table, sizeof(seg_table));
    seg_table[crt_tls_seg_off/sizeof(capability)] = tls_seg;

    memcpy(tls_seg, crt_tls_proto, crt_tls_proto_size);

    crt_init_new_locals(seg_table, RELOCS_START, RELOCS_END);

    CTL_t* locals = (CTL_t*)((char*)tls_seg + crt_cap_tab_local_addr);

    locals->cds = get_ctl()->cds;
    locals->cgp = get_ctl()->cgp;
    locals->cdl = &entry_stub;
    locals->csp = stack;
    locals->cusp = ustack;
    locals->guard.guard = callable_ready;

    capability sealed_locals =  cheri_seal(locals, locals->cds);

    // Now sign
    single_use_cert cert = rescap_take_authed(sign_res, NULL, 0, AUTH_SINGLE_USE_CERT, own_auth, NULL, sealed_locals).scert;

    return cert;
}

// This is called via ccall to finish link
int __attribute__((used)) INIT_OTHER_OBJECT(LIB_IF_T)(act_control_kt self_ctrl, mop_t mop, queue_t* queue, startup_flags_e start_flags) {
    mmap_set_mop(mop);
    object_init(self_ctrl, queue, NULL, NULL, start_flags, 0);
    if(own_stats == NULL) {
        own_stats = syscall_act_user_info_ref(self_ctrl);
    }
    return 0;
}

void (*msg_methods[]) = {get_size_for_new, get_if_cert, create_new_external_thread};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, ctor_null, dtor_null};
size_t ctrl_methods_nb = countof(ctrl_methods);

int server_start(void) {
    // This library must be secure loaded
    assert(was_secure_loaded);

    // Get a type for this domain (all secure loaded things are given a domain sealer automatically)
    sealing_cap sc = get_ctl()->cds;

    // Make (and sign) an interface

    size_t size_needed = RES_CERT_META_SIZE + sizeof(LIB_IF_T);

    res_t if_res = mem_request(0, size_needed, NONE, own_mop).val;

    rescap_split(if_res, size_needed);

    _safe cap_pair pair;

    if_cert = rescap_take_authed(if_res, &pair, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP, AUTH_CERT, own_auth, NULL, NULL).cert;

    assert(if_cert != NULL);

    LIB_IF_T* interface = (LIB_IF_T*)pair.data;

#define ASSIGN_TO_IF(name, ret, sig, table, ...) table -> name = cheri_seal(&CROSS_DOMAIN(name), sc);
    LIB_IF_LIST(ASSIGN_TO_IF, interface)



    // Go into daemon mode
    msg_enable = 1;

    return 0;
}