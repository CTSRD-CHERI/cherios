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

#ifndef CHERIOS_PROC_H
#define CHERIOS_PROC_H

#include "elf.h"
#include "mman.h"
#include "tman.h"

#define MAX_PROCS 0x20

enum process_state {
    proc_created = 0,
    proc_started = 1,
    proc_zombie = 2
};

typedef struct process_t {
    enum process_state state;
    const char* name; // Or some other appropriate i.d.
    image im;
    mop_t mop;
    top_t top;
    act_control_kt threads[MAX_THREADS];
    size_t n_threads;
    size_t terminated_threads;
} process_t;


process_t* seal_proc_for_user(process_t* process);
process_t* unseal_proc(process_t* process);

act_control_kt create_thread(process_t * process, const char* name, register_t arg, capability carg, capability pcc,
                             char* stack_args, size_t stack_args_size, uint8_t cpu_hint);

act_control_kt start_process(process_t* proc,
                             register_t arg, capability carg, char* stack_args, size_t stack_args_size, uint8_t cpu_hint);

process_t* create_process(const char* name, capability file, int secure_load);

#endif //CHERIOS_PROC_H
