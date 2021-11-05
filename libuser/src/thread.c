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

#include "spinlock.h"
#include "thread.h"
#include "cheric.h"
#include "syscalls.h"
#include "stdio.h"
#include "namespace.h"
#include "assert.h"
#include "msg.h"
#include "object.h"
#include "nano/nanokernel.h"
#include "capmalloc.h"
#include "crt.h"
#include "string.h"
#include "tman.h"
#include "cprogram.h"
#include "stdlib.h"

act_kt proc_man_ref = NULL;
process_kt proc_handle = NULL;
sealing_cap cds_for_new_threads;

extern void thread_start(void);
extern void secure_thread_start(void);

extern link_session_t own_link_session;

void c_thread_call_start(register_t arg, capability carg, thread_start_func_t* start, __unused capability clean_me_up) {

    // TODO: Clean up argument if one is provided. Cant do this right now due to broken cross thread free

    start(arg, carg);

    main_returns();
}

capability c_thread_start(capability* data_args, capability* segment_table, capability tls_segment_prototype, register_t tls_segment_offset,
                    queue_t* queue, act_control_kt self_ctrl, startup_flags_e flags,
                    kernel_if_t* kernel_if_c) {
    // We have to do this before we can get any thread locals
    memcpy(segment_table[tls_segment_offset/sizeof(capability)], tls_segment_prototype, crt_tls_proto_size);

    // The __stop___cap_relocs will be incorrect as it doesn't have size and so compaction fluffs it up =(

    struct capreloc* r_start = RELOCS_START;
    struct capreloc* r_stop = RELOCS_END;

    crt_init_new_locals(segment_table, r_start, r_stop);

    get_ctl()->cds = cds_for_new_threads;

    object_init(self_ctrl, queue, kernel_if_c, NULL, flags, 0);

#ifndef LIB_EARLY
    auto_dylink_post_new_thread(&own_link_session, data_args);
#else
    (void)(data_args);
#endif

    // Return the function to call start rather than call it directly to allow stack recovery
    return (capability)&c_thread_call_start;
}

process_kt thread_create_process(const char* name, const char* file, int secure_load) {
    if(proc_man_ref == NULL) {
        proc_man_ref = namespace_get_ref(namespace_num_proc_manager);
    }
    assert(proc_man_ref != NULL);
    return message_send_c(secure_load, 0, 0, 0, __DECONST(capability,name), __DECONST(capability,file), NULL, NULL, proc_man_ref, SYNC_CALL, 0);
}
thread thread_start_process(process_kt proc, startup_desc_t* desc) {
    if(proc_man_ref == NULL) {
        proc_man_ref = namespace_get_ref(namespace_num_proc_manager);
    }
    assert(proc_man_ref != NULL);
    return message_send_c(0, 0, 0, 0, proc, desc, NULL, NULL, proc_man_ref, SYNC_CALL, 1);
}
thread thread_create_thread(process_kt proc, const char* name, startup_desc_t* desc) {
    if(proc_man_ref == NULL) {
        proc_man_ref = namespace_get_ref(namespace_num_proc_manager);
    }
    assert(proc_man_ref != NULL);
    return message_send_c(0, 0, 0, 0, proc, __DECONST(char*,name), desc, NULL, proc_man_ref, SYNC_CALL, 2);
}

top_t own_top;

top_t get_own_top(void) {
    if(!own_top) {
        try_init_tman_ref();
        own_top = get_top_for_process(proc_handle);
    }
    return own_top;
}

top_t get_top_for_process(process_kt proc) {
    if(proc_man_ref == NULL) {
        proc_man_ref = namespace_get_ref(namespace_num_proc_manager);
    }
    assert(proc_man_ref != NULL);
    return (top_t)message_send_c(0, 0, 0, 0, proc, NULL, NULL, NULL, proc_man_ref, SYNC_CALL, 5);
}

capability get_type_owned_by_process(void) {
    own_top = get_own_top();
    if(own_top == NULL)return NULL;
    ERROR_T(tres_t) res = type_get_new(own_top);
    if(!IS_VALID(res)) return NULL;
    return tres_take(res.val);
}

thread thread_new_hint(const char* name, register_t arg, capability carg, thread_start_func_t* start, uint8_t cpu_hint) {
    struct start_stack_args args;
    startup_desc_t startup;

    startup.cpu_hint = cpu_hint;

    startup.flags = default_flags;

    capability* data_args;

    if(!was_secure_loaded) {

        args.start = start;

        startup.stack_args_size = sizeof(struct start_stack_args);
        startup.stack_args = (capability)&args;
        startup.carg = carg;
        startup.arg = arg;
        startup.pcc = &thread_start;

        data_args = args.data_args;

    } else {

        // New threads created by a foundation are started in foundation mode
        // Can't allocate these as one block, this would make setting bounds hard

        capability stack = malloc(DEFAULT_STACK_SIZE_NO_QUEUE);
        capability tls_seg = malloc(crt_tls_seg_size);

        size_t space_required =  RES_CERT_META_SIZE +           // to lock
                                sizeof(struct secure_start_t);  // for our fields

        res_t res = cap_malloc(space_required);

        _safe cap_pair pair;
        invocable_t invocable = rescap_take_authed(res, &pair, CHERI_PERM_ALL, AUTH_INVOCABLE, own_auth, NULL, NULL).invocable;

        struct secure_start_t* start_message = ( struct secure_start_t*)pair.data;

        stack = cheri_incoffset(stack, DEFAULT_STACK_SIZE_NO_QUEUE);

        start_message->carg = carg;
        start_message->arg = arg;
        start_message->start = start;
        spinlock_init(&start_message->once);

        start_message->c10 = NULL;
        start_message->c11 = stack;
        start_message->cgp = get_cgp();

        size_t locals_len = cheri_getlen(cheri_getidc());
        size_t locals_off = crt_cap_tab_local_addr;

        start_message->idc = cheri_setbounds((char*)tls_seg+locals_off, locals_len);

        memcpy(start_message->segment_table, crt_segment_table, sizeof(crt_segment_table));
        start_message->segment_table[crt_tls_seg_off/sizeof(capability)] = tls_seg;

        startup.stack_args_size = 0;
        startup.stack_args = NULL;
        startup.inv = invocable;
        startup.carg = NULL;
        startup.arg = 0; // DUMMY, passed through the start_message
        startup.pcc = NULL; // DUMMY, passed through the start_message

        data_args = start_message->data_args;
    }

#ifndef LIB_EARLY
    auto_dylink_pre_new_thread(&own_link_session, data_args);
#else
    (void)data_args;
#endif

    return thread_create_thread(proc_handle, name, &startup);
}

thread thread_new(const char* name, register_t arg, capability carg, thread_start_func_t* start) {
    return thread_new_hint(name, arg, carg, start, 0);
}

void thread_init(void) {
    if(was_secure_loaded) {
        // New threads in secure load mode should go through secure_thread_start
        cds_for_new_threads = get_ctl()->cds;
        __unused entry_t e = foundation_new_entry(0, &secure_thread_start, own_auth);
        assert(e != NULL);
    }
}
