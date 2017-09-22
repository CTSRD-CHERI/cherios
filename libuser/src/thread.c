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

act_kt* proc_man_ref = NULL;
process_kt proc_handle = NULL;

extern void thread_start(void);
extern void secure_thread_start(void);
extern void msg_entry(void);

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

__asm__ (
    ".text\n"
    ".global thread_start\n"
    "thread_start:\n"
    "cmove $c4, $c25\n"
    "cmove $c5, $c21\n"
    "clc   $c6, $sp, " HELP(START_OFF) "($c11)\n"
    "daddiu $sp, $sp, " HELP(ARGS_SIZE) "\n"
    "dla    $t0, c_thread_start\n"
    "cgetpccsetoffset $c12, $t0\n"
    "cjr    $c12\n"
);

// A trampoline that arrives

__asm__ (
        ".text\n"
        ".global secure_thread_start\n"
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
);

void c_thread_start(register_t arg, capability carg, queue_t* queue, act_control_kt self_ctrl, thread_start_func_t* start) {
    object_init(self_ctrl, queue, NULL);

    start(arg, carg);

    if(msg_enable) {
        msg_entry();
    } else {
        syscall_act_terminate(self_ctrl);
    }
}

void secure_c_thread_start(locked_t locked_start, queue_t* queue, act_control_kt self_ctrl) {

    assert(locked_start != NULL && (cheri_gettype(locked_start) == FOUND_LOCKED_TYPE));

    cap_pair pair;

    rescap_unlock(locked_start, &pair);

    assert(pair.data != NULL);

    struct secure_start_t* start = (struct secure_start_t*)pair.data;

    int taken = spinlock_try_acquire(&start->once);

    // protects against double entry
    assert(taken);

    c_thread_start(start->arg, start->carg, queue, self_ctrl, start->start);
}

process_kt thread_create_process(const char* name, capability file, int secure_load) {
    if(proc_man_ref == NULL) {
        proc_man_ref = namespace_get_ref(namespace_num_proc_manager);
    }
    assert(proc_man_ref != NULL);
    return message_send_c(secure_load, 0, 0, 0, name, file, NULL, NULL, proc_man_ref, SYNC_CALL, 0);
}
thread thread_start_process(process_kt* proc, startup_desc_t* desc) {
    if(proc_man_ref == NULL) {
        proc_man_ref = namespace_get_ref(namespace_num_proc_manager);
    }
    assert(proc_man_ref != NULL);
    return message_send_c(0, 0, 0, 0, proc, desc, NULL, NULL, proc_man_ref, SYNC_CALL, 1);
}
thread thread_create_thread(process_kt* proc, const char* name, startup_desc_t* desc) {
    if(proc_man_ref == NULL) {
        proc_man_ref = namespace_get_ref(namespace_num_proc_manager);
    }
    assert(proc_man_ref != NULL);
    return message_send_c(0, 0, 0, 0, proc, name, desc, NULL, proc_man_ref, SYNC_CALL, 2);
}

thread thread_new(const char* name, register_t arg, capability carg, thread_start_func_t* start) {
    struct start_stack_args args;
    startup_desc_t startup;

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

void thread_init(void) {
    if(was_secure_loaded) {
        // New threads in secure load mode should go through secure_thread_start
        entry_t e = foundation_new_entry(0, &secure_thread_start);
        assert(e != NULL);
    }
}