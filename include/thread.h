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

#ifndef CHERIOS_UNISTD_H_H
#define CHERIOS_UNISTD_H_H

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

#define START_OFF       0
#define DATA_ARGS_OFF   CAP_SIZE

#ifndef __ASSEMBLY__

#include "types.h"
#include "queue.h"
#include "tman.h"
#include "spinlock.h"
#include "dylink.h"

typedef enum startup_flags_e {
    STARTUP_NONE            = 0x00,
    STARTUP_NO_DEDUP        = 0x01,
    STARTUP_NO_COMPACT      = 0x02,
    STARTUP_NO_EXCEPTIONS   = 0x04,
    STARTUP_NO_MALLOC       = 0x08,
    STARTUP_NO_THREADS      = 0x10,

    STARTUP_BASIC           = STARTUP_NO_DEDUP | STARTUP_NO_COMPACT | STARTUP_NO_EXCEPTIONS |
                                STARTUP_NO_MALLOC | STARTUP_NO_THREADS,
} startup_flags_e;

typedef void pcc_type(void);
typedef struct startup_desc_t {
    capability carg;
    pcc_type* pcc;
    char* stack_args;
    invocable_t inv; // only for secure loaded things
    register_t arg;
    size_t stack_args_size;
    startup_flags_e flags;
    uint8_t cpu_hint;
} startup_desc_t;

/* The type of a start function */
typedef void thread_start_func_t(register_t arg, capability carg);

/* Map a user thread onto a single activation */
typedef act_control_kt thread;

static inline act_control_kt get_control_for_thread(thread t) {
    return (act_control_kt)t;
}

/* We will get sealed handles from the process manager that represent a process */
typedef capability process_kt;

/* A global reference to the handle for the process this thread belongs to */
extern process_kt proc_handle;

#define thread_handle ((thread)act_self_ctrl)

/* Create a thread with the same c0 and pcc capabilities as the caller,
 * but with its own stack and queue and ref/ctrl ref */
thread thread_new(const char* name, register_t arg, capability carg, thread_start_func_t* start);
/* Same but with a hint to schedule on a particular CPU */
thread thread_new_hint(const char* name, register_t arg, capability carg, thread_start_func_t* start, uint8_t cpu_hint);

void thread_init(void);

/* These are wrappers for messages to the process manager */

process_kt thread_create_process(const char* name, const char* file, int secure_load);
thread thread_start_process(process_kt proc, startup_desc_t* desc);
thread thread_create_thread(process_kt proc, const char* name, startup_desc_t* desc);

top_t get_top_for_process(process_kt proc);
top_t get_own_top(void);
capability get_type_owned_by_process(void);

/* These are used internally */

// TODO RISCV: This struct will be the same for RISCV, but like in other places, some fields are badly named now

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

struct start_stack_args {
    thread_start_func_t* start;
    capability data_args[MAX_LIBS+1];
};

#endif

#endif //CHERIOS_UNISTD_H_H
