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

struct start_stack_args {
    thread_start_func_t* start;
};

// This will be symetric locked. It can only be created by the foundation, and only read by the foundation.
// We try do all the hard bootstrapping in the parent whilst C is a available and then just load in the new thread.
struct secure_start_t {
    capability idc;

    // We will use this as an idc while setting up. If we want user exceptions (we currently don't) these fields can be set
    // WARN: The offsets of these 3 fields are very important. Don't move them.
    ex_pcc_t* ex_pcc;
    capability ex_idc;
    capability ex_c1;

    capability cgp;
    capability c11;
    capability c10;

    thread_start_func_t * start;
    capability carg;
    register_t arg;
    spinlock_t once;
    capability segment_table[MAX_SEGS];
};


#define IDC_OFF 0
#define CGP_OFF (4 * CAP_SIZE)
#define C11_OFF (5 * CAP_SIZE)
#define C10_OFF (6 * CAP_SIZE)

#define START_OFF_SEC (7 * CAP_SIZE)
#define CARG_OFF_SEC (8 * CAP_SIZE)
#define ARG_OFF_SEC (9 * CAP_SIZE)

#define SPIN_OFF ((9 * CAP_SIZE) +  REG_SIZE)
#define SEG_TBL_OFF (10 * CAP_SIZE)

_Static_assert((offsetof(struct secure_start_t, once)) == SPIN_OFF, "used by assembly below");
_Static_assert((offsetof(struct secure_start_t, segment_table)) == SEG_TBL_OFF, "used by assembly below");

#define START_OFF 0
#define CARG_OFF 0
#define ARG_OFF 0
#define NONCE_OFF 0

#define ARGS_SIZE (CAP_SIZE)

#define STRFY(X) #X
#define HELP(X) STRFY(X)

_Static_assert((offsetof(struct start_stack_args, start)) == START_OFF, "used by assembly below");
_Static_assert((sizeof(struct start_stack_args)) == ARGS_SIZE, "used by assembly below");


// These will be called instead of normal init for new threads
// Check init.S in libuser for convention. Most relocations will have been processed - but we need to do locals again
// This trampoline constructs a locals and globals captable and then moves straight into c
__asm__ (
    SANE_ASM
    ".text\n"
    ".global thread_start           \n"
    ".ent thread_start              \n"
    "thread_start:                  \n"
    "clc         $c13, $a2, 0($c4)  \n"

// Get globals
    "dla         $t0, __cap_table_start                     \n"
    "dsubu       $t0, $t0, $a3                              \n"
    "cincoffset  $c25, $c13, $t0                            \n"
    "clcbi       $c25, %captab20(__cap_table_start)($c25)   \n"

// Get locals
    "dla         $t0, __cap_table_local_start   \n"
    "dsubu       $t0, $t0, $a6                  \n"
    "clc         $c26, $s1, 0($c4)              \n"
    "cincoffset  $c26, $c26, $t0                \n"
    "clcbi       $c13, %captab20(__cap_table_local_start)($c25) \n"
    "cgetlen     $t0, $c13                      \n"
    "csetbounds  $c26, $c26, $t0                \n"

    // c3 already carg and a0 already arg
    // c4 already segment_table
    // c5 already tls_prototype
    "move       $a1, $s1    \n"     // tls_segment
    "cmove      $c6, $c20   \n"     // queue
    "cmove      $c7, $c21   \n"     // self ctrl
    "clc        $c8, $zero, " HELP(START_OFF) "($c11)\n"
    "cmove      $c9, $c24   \n"     // kernel_if_t
    "move       $a2, $s2    \n"    // startup flags
    // Call c land now globals are set up
    "clcbi   $c12, %capcall20(c_thread_start)($c25)\n"
    "cjr     $c12\n"
    "cincoffset  $c11, $c11, " HELP(ARGS_SIZE) "\n" // todo reclaim seg table
    ".end thread_start"
);

__asm__ (
    SANE_ASM
        ".text\n"
        ".global secure_thread_start\n"
        ".ent secure_thread_start\n"
        "secure_thread_start:\n"
    // Must have provided an invocation
        "cbtu       $idc, fail\n"
        "cincoffset $c14, $idc, " HELP(SPIN_OFF) "\n"
    // Obtain lock or fail
        "li         $t0, 1\n"
        "1: cllb    $t1, $c14\n"
        "bnez       $t1, fail\n"
        "cscb       $t1, $t0, $c14\n"
        "beqz       $t1, 1b\n"
        "cmove      $c13, $idc\n"
    // Load stacks(s)
        "clc        $c11, $zero, (" HELP(C11_OFF) ")($c13)\n"
        "clc        $c10, $zero, (" HELP(C10_OFF) ")($c13)\n"
    // Load globals
        "clc        $c25, $zero, (" HELP(CGP_OFF) ")($c13)\n"
    // Load idc (at this point we will take exceptions as the caller intended)
        "clc        $idc, $zero, (" HELP(IDC_OFF) ")($c13)\n"
    // Now call the same thread_start func
        "cld        $a0 , $zero, (" HELP(ARG_OFF_SEC) ")($c13)\n" // arg
        "clc        $c3 , $zero, (" HELP(CARG_OFF_SEC) ")($c13)\n" // carg
        "clcbi      $c14, %captab20(crt_tls_seg_off)($c25)\n" // tls seg offset
        "cld        $a1, $zero, 0($c14)\n"
        "cincoffset $c4 , $c13, (" HELP(SEG_TBL_OFF) ")\n" // seg table
        "csetbounds $c4, $c4," HELP(CAP_SIZE * MAX_SEGS) "\n" // and bound it
        "clcbi      $c5, %captab20(crt_tls_proto)($c25)\n" // tls_proto
        "clc        $c5, $zero, 0($c5)\n"
        "cmove      $c6, $c20\n" // queue (make a new one straight away?)
        "cmove      $c7, $c21\n" // self ctrl
        "clc        $c8 , $zero, (" HELP(START_OFF_SEC) ")($c13)\n" // start
        "cmove      $c9, $c24   \n"     // kernel_if_t
        "clcbi   $c12, %capcall20(c_thread_start)($c25)\n"
        "cjr        $c12\n"
        "move       $a2, $s2    \n"    // startup flags
        "fail:      teqi $zero, 0\n"
        "nop\n"
        ".end secure_thread_start\n"
);

// TODO we now have some globals for some of these arguments. Use those instead?
void c_thread_start(register_t arg, capability carg, // Things from the user
                    capability* segment_table, capability tls_segment_prototype, register_t tls_segment_offset,
                    queue_t* queue, act_control_kt self_ctrl, thread_start_func_t* start, startup_flags_e flags,
                    kernel_if_t* kernel_if_c) {
    // We have to do this before we can get any thread locals
    memcpy(segment_table[tls_segment_offset/sizeof(capability)], tls_segment_prototype, crt_tls_proto_size);

    // The __stop___cap_relocs will be incorrect as it doesn't have size and so compaction fluffs it up =(

    struct capreloc* r_start = &__start___cap_relocs;
    struct capreloc* r_stop = cheri_incoffset(r_start, cap_relocs_size);

    crt_init_new_locals(segment_table, r_start, r_stop);

    get_ctl()->cds = cds_for_new_threads;

    object_init(self_ctrl, queue, kernel_if_c, NULL, flags, 0);

    start(arg, carg);

    if(msg_enable) {
        msg_entry(0);
    } else {
        object_destroy();
    }
}

act_control_kt get_control_for_thread(thread t) {
    return (act_control_kt)t;
}

process_kt thread_create_process(const char* name, capability file, int secure_load) {
    if(proc_man_ref == NULL) {
        proc_man_ref = namespace_get_ref(namespace_num_proc_manager);
    }
    assert(proc_man_ref != NULL);
    return message_send_c(secure_load, 0, 0, 0, name, file, NULL, NULL, proc_man_ref, SYNC_CALL, 0);
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
    return message_send_c(0, 0, 0, 0, proc, name, desc, NULL, proc_man_ref, SYNC_CALL, 2);
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

    if(!was_secure_loaded) {

        args.start = start;

        startup.stack_args_size = sizeof(struct start_stack_args);
        startup.stack_args = (capability)&args;
        startup.carg = carg;
        startup.arg = arg;
        startup.pcc = &thread_start;


    } else {

        // New threads created by a foundation are started in foundation mode
        // Can't allocate these as one block, this would make setting bounds hard

        capability stack = malloc(DEFAULT_STACK_SIZE);
        capability tls_seg = malloc(crt_tls_seg_size);

        size_t space_required =  RES_CERT_META_SIZE +           // to lock
                                sizeof(struct secure_start_t);  // for our fields

        res_t res = cap_malloc(space_required);

        cap_pair pair;
        cert_t locked = rescap_take_cert(res, &pair, CHERI_PERM_ALL, 1, own_auth);

        struct secure_start_t* start_message = ( struct secure_start_t*)pair.data;

        stack = cheri_incoffset(stack, DEFAULT_STACK_SIZE);

        start_message->carg = carg;
        start_message->arg = arg;
        start_message->start = start;
        spinlock_init(&start_message->once);

        start_message->c10 = NULL;
        start_message->c11 = stack;
        start_message->cgp = get_cgp();

        size_t locals_len = cheri_getlen(get_idc());
        size_t locals_off = crt_cap_tab_local_addr;

        start_message->idc = cheri_setbounds(tls_seg+locals_off, locals_len);

        memcpy(start_message->segment_table, crt_segment_table, sizeof(crt_segment_table));
        start_message->segment_table[crt_tls_seg_off/sizeof(capability)] = tls_seg;

        startup.stack_args_size = 0;
        startup.stack_args = NULL;
        startup.cert = locked;
        startup.carg = NULL;
        startup.arg = 0; // DUMMY, passed through the start_message
        startup.pcc = NULL; // DUMMY, passed through the start_message
    }

    return thread_create_thread(proc_handle, name, &startup);
}

thread thread_new(const char* name, register_t arg, capability carg, thread_start_func_t* start) {
    return thread_new_hint(name, arg, carg, start, 0);
}

void thread_init(void) {
    if(was_secure_loaded) {
        // New threads in secure load mode should go through secure_thread_start
        cds_for_new_threads = get_ctl()->cds;
        entry_t e = foundation_new_entry(0, &secure_thread_start, own_auth);
        assert(e != NULL);
    }
}