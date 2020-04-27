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
    capability data_args[MAX_LIBS+1];
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
    capability data_args[MAX_LIBS+1];
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
#define DATA_ARGS_OFF_SEC (SEG_TBL_OFF + (MAX_SEGS * CAP_SIZE))

_Static_assert((offsetof(struct secure_start_t, once)) == SPIN_OFF, "used by assembly below");
_Static_assert((offsetof(struct secure_start_t, segment_table)) == SEG_TBL_OFF, "used by assembly below");
_Static_assert((offsetof(struct secure_start_t, data_args)) == DATA_ARGS_OFF_SEC, "used by assembly below");

#define START_OFF       0
#define DATA_ARGS_OFF   CAP_SIZE


#define STRFY(X) #X
#define HELP(X) STRFY(X)

_Static_assert((offsetof(struct start_stack_args, start)) == START_OFF, "used by assembly below");
_Static_assert((offsetof(struct start_stack_args, data_args)) == DATA_ARGS_OFF, "used by assembly below");

#define LOAD_VADDR(reg, offset_reg) \
    "dsrl        " reg ", " offset_reg ", (" HELP(CAP_SIZE_BITS) " - " HELP(REG_SIZE_BITS) ") \n" \
    "cld         " reg ", " reg ", (" HELP(MAX_SEGS)" * " HELP(CAP_SIZE) ")($c4) \n"


// data_args = c3, segment_table = c4, tls_segment_proto = c5, tls_segment_off = a0,
// queue = c6, self_ctrl = c7, flags = a1, if_c = c8


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
LOAD_VADDR("$a6", "$s1")
LOAD_VADDR("$a3", "$a2")
// Get globals
    cheri_dla_asm("$t0", "__cap_table_start")
    "dsubu       $t0, $t0, $a3                              \n"
    "cincoffset  $c25, $c13, $t0                            \n"
    "clcbi       $c25, %captab20(__cap_table_start)($c25)   \n"
// Get locals
    cheri_dla_asm("$t0", "__cap_table_local_start")
    "dsubu       $t0, $t0, $a6                  \n"
    "clc         $c26, $s1, 0($c4)              \n"
    "cincoffset  $c26, $c26, $t0                \n"
    "clcbi       $c13, %captab20(__cap_table_local_start)($c25) \n"
    "cgetlen     $t0, $c13                      \n"
    "csetbounds  $c26, $c26, $t0                \n"

    // Save c3 and a0

    "move       $s0, $a0    \n"
    "cmove      $c19, $c3   \n"

    // c4 already segment_table
    // c5 already tls_prototype
    "clcbi   $c12, %capcall20(c_thread_start)($c25)\n"
    "cincoffset $c3, $c11, " HELP(DATA_ARGS_OFF) "\n" // data_args
    "move       $a0, $s1    \n"     // tls_segment
    "cmove      $c6, $c20   \n"     // queue
    "cmove      $c7, $c21   \n"     // self ctrl
    "cmove      $c8, $c24   \n"     // kernel_if_t
    "move       $a1, $s2    \n"     // startup flags
    // Call c land now globals are set up
    "cjalr      $c12, $c17  \n"
    "cmove      $c18, $idc  \n"
    // Reset stack then finish calling start with restored arguments
    "cmove      $c12, $c3           \n"
    "clc        $c4, $zero, " HELP(START_OFF) "($c11)\n"
    "cmove      $c3, $c19           \n"
    "move       $s0, $a0            \n"
    "cmove      $c5, $cnull         \n"
    "cgetlen    $at, $c11           \n"
    "cjalr      $c12, $c17  \n"
    "csetoffset $c11, $c11, $at     \n"
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
        "cmove      $c19, $idc\n"
    // Load stacks(s)
        "clc        $c11, $zero, (" HELP(C11_OFF) ")($c19)\n"
        "clc        $c10, $zero, (" HELP(C10_OFF) ")($c19)\n"
    // Load globals
        "clc        $c25, $zero, (" HELP(CGP_OFF) ")($c19)\n"
    // Load idc (at this point we will take exceptions as the caller intended)
        "clc        $idc, $zero, (" HELP(IDC_OFF) ")($c19)\n"
    // Now call the same thread_start func
        "clcbi      $c14, %captab20(crt_tls_seg_off)($c25)\n" // tls seg offset ptr
        "cld        $a0, $zero, 0($c14)\n"                  // tls seg offset
        "cincoffset $c4 , $c19, (" HELP(SEG_TBL_OFF) ")\n" // seg table
        "csetbounds $c4, $c4," HELP(CAP_SIZE * MAX_SEGS) "\n" // and bound it
        "clcbi      $c5, %captab20(crt_tls_proto)($c25)\n" // tls_proto
        "clc        $c5, $zero, 0($c5)\n"
        "cmove      $c6, $c20\n" // queue (make a new one straight away?)
        "cmove      $c7, $c21\n" // self ctrl
        "clcbi   $c12, %capcall20(c_thread_start)($c25)\n"
        "cmove      $c8, $c24   \n"     // kernel_if_t
        "cincoffset $c3 , $c19, (" HELP(DATA_ARGS_OFF_SEC) ")\n" // data arg table
        "move       $a1, $s2    \n"    // startup flags
        "cjalr      $c12, $c17  \n"
        "cmove      $c18, $idc  \n"
// Finish calling start. Clean up will be done for us (argument c5)
        "cmove      $c12, $c3           \n"
        "cld        $a0 , $zero, (" HELP(ARG_OFF_SEC) ")($c19)\n" // arg
        "clc        $c3 , $zero, (" HELP(CARG_OFF_SEC) ")($c19)\n" // carg
        "clc        $c4 , $zero, (" HELP(START_OFF_SEC) ")($c19)\n" // start
        "cjalr      $c12, $c17  \n"
        "cmove      $c5, $c19    \n"        // Get callee to clean up this object
        "fail:      teqi $zero, 0\n"
        "nop\n"
        ".end secure_thread_start\n"
);

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

process_kt thread_create_process(const char* name, capability file, int secure_load) {
    if(proc_man_ref == NULL) {
        proc_man_ref = namespace_get_ref(namespace_num_proc_manager);
    }
    assert(proc_man_ref != NULL);
    return message_send_c(secure_load, 0, 0, 0, __DECONST(capability,name), file, NULL, NULL, proc_man_ref, SYNC_CALL, 0);
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

        size_t locals_len = cheri_getlen(get_idc());
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