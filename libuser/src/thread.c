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

act_kt proc_man_ref = NULL;
process_kt proc_handle = NULL;

extern void thread_start(void);
extern void secure_thread_start(void);

struct start_stack_args {
    thread_start_func_t* start;
};

struct secure_start_t {
    thread_start_func_t * start;
    capability carg;
    register_t arg;
    spinlock_t once;
};

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
    "move       $a2, $s2    \n"    // startup flags
    // Call c land now globals are set up
    "clcbi   $c12, %capcall20(c_thread_start)($c25)\n"
    "cjr     $c12\n"
    "cincoffset  $c11, $c11, " HELP(ARGS_SIZE) "\n"
    ".end thread_start"
);

// FIXME: Just wont work with the new ABI

__asm__ (
    SANE_ASM
        ".text\n"
        ".global secure_thread_start\n"
        ".ent secure_thread_start\n"
        "secure_thread_start:\n"
// Undo permutation that happened in the foundation enter trampoline
        "cmove       $c4, $idc\n"
        "cmove       $idc, $c3\n"
        "cgetdefault $c3\n"
        "csetdefault $c4\n"
        "cmove $c4, $c25\n"
        "cmove $c5, $c21\n"
        "dla    $t0, secure_c_thread_start\n"
        "cgetpccsetoffset $c12, $t0\n"
        "cjr    $c12\n"
        "nop\n"
        ".end secure_thread_start\n"
);

void c_thread_start(register_t arg, capability carg, // Things from the user
                    capability* segment_table, capability tls_segment_prototype, register_t tls_segment_offset,
                    queue_t* queue, act_control_kt self_ctrl, thread_start_func_t* start, startup_flags_e flags) {
    // We have to do this before we can get any thread locals
    memcpy(segment_table[tls_segment_offset/sizeof(capability)], tls_segment_prototype, cheri_getlen(tls_segment_prototype));

    struct capreloc* r_start = &__start___cap_relocs;

    // Deduplication makes different symbols not comparable like this. Weirdly stop had different bounds to start.
    //struct capreloc* r_stop = cheri_incoffset(r_start, (size_t)&__stop___cap_relocs - (size_t)&__start___cap_relocs);
    // FIXME: Allows works with precise bounds (which we hopefully have) =/
    struct capreloc* r_stop = cheri_setoffset(r_start, cheri_getlen(r_start));
    crt_init_new_locals(segment_table, r_start, r_stop);

    object_init(self_ctrl, queue, NULL, NULL, flags);

    start(arg, carg);

    if(msg_enable) {
        msg_entry(0);
    } else {
        syscall_act_terminate(self_ctrl);
    }
}

void secure_c_thread_start(locked_t locked_start, queue_t* queue, act_control_kt self_ctrl) {

    assert(0 && "Secure load no longer works");

    assert(locked_start != NULL && (cheri_gettype(locked_start) == FOUND_LOCKED_TYPE));

    cap_pair pair;

    rescap_unlock(locked_start, &pair);

    assert(pair.data != NULL);

    struct secure_start_t* start = (struct secure_start_t*)pair.data;

    // FIXME: Compiler bug?
//int taken = spinlock_try_acquire(&start->once, 10);

    int taken = 1;

    // protects against double entry
    assert(taken);

    // c_thread_start(start->arg, start->carg, queue, self_ctrl, start->start);
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

    if(!was_secure_loaded) {

        args.start = start;

        startup.stack_args_size = sizeof(struct start_stack_args);
        startup.stack_args = (capability)&args;
        startup.carg = carg;
        startup.arg = arg;
        startup.pcc = &thread_start;


    } else {

        // New threads created by a foundation are started in foundation mode

        res_t res = cap_malloc(sizeof(struct secure_start_t) + RES_CERT_META_SIZE);

        cap_pair pair;
        locked_t locked = rescap_take_locked(res, &pair, CHERI_PERM_ALL, own_found_id);
        struct secure_start_t* start_message = ( struct secure_start_t*)pair.data;

        start_message->carg = carg;
        start_message->arg = arg;
        start_message->start = start;
        spinlock_init(&start_message->once);

        startup.stack_args_size = 0;
        startup.stack_args = NULL;
        startup.carg = locked;
        startup.arg = 0;
        startup.pcc = NULL;
    }

    return thread_create_thread(proc_handle, name, &startup);
}

thread thread_new(const char* name, register_t arg, capability carg, thread_start_func_t* start) {
    return thread_new_hint(name, arg, carg, start, 0);
}

void thread_init(void) {
    if(was_secure_loaded) {
        // New threads in secure load mode should go through secure_thread_start
        entry_t e = foundation_new_entry(0, &secure_thread_start);
        assert(e != NULL);
    }
}