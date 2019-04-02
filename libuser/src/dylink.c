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

#include "assert.h"
#include "msg.h"
#include "syscalls.h"
#include "thread.h"
#include "nano/nanokernel.h"
#include "capmalloc.h"
#include "mman.h"
#include "cprogram.h"
#include "temporal.h"
#include "stdio.h"
#include "dylink_client.h"

void dylink(act_control_kt self_ctrl, queue_t * queue, startup_flags_e startup_flags, int first_thread,
        act_kt dylink_server, init_if_func_t* init_if_func, init_if_new_thread_func_t* init_if_new_thread_func,
        init_other_object_func_t * init_other_object) {

    assert(dylink_server != NULL);

    cap_pair pair;
    capability lib_if;
    found_id_t* id;

    if(first_thread) {
        cert_t if_cert = message_send_c(0, 0, 0, 0, NULL, NULL, NULL, NULL, dylink_server, SYNC_CALL, DYLINK_IPC_NO_GET_IF);
        id = rescap_check_cert(if_cert, &pair);
        assert(id != NULL);
        lib_if = pair.data;
    }

    size_t size = message_send(0, 0, 0, 0, NULL, NULL, NULL, NULL, dylink_server, SYNC_CALL, DYLINK_IPC_NO_GET_TABLE_SIZE);

    assert(size != 0);

    // Allocate space / stacks for new thread in other library
    res_t locals_res, stack_res, ustack_res, sign_res;
    locals_res = cap_malloc(size);
    stack_res = mem_request(0, DEFAULT_STACK_SIZE, 0, own_mop).val;
    ustack_res = mem_request(0, Overrequest + MinStackSize, 0, own_mop).val;
    sign_res = cap_malloc(RES_CERT_META_SIZE);

    // Create a new thread in target library
    single_use_cert thread_cert = message_send_c(0, 0, 0, 0, locals_res, stack_res, ustack_res, sign_res, dylink_server, SYNC_CALL, DYLINK_IPC_NO_GET);

    // TODO. If we would only accept a certain set of libraries, we would check the sig here
    found_id_t* id2 = rescap_check_single_cert(thread_cert, &pair);

    capability dataarg = pair.data;

    if(first_thread) {
        // Needs same interface
        if(id != id2) {
            CHERI_PRINT_CAP(id);
            CHERI_PRINT_CAP(id2);
            CHERI_PRINT_CAP(thread_cert);
        }
        assert(id == id2);
        init_if_func(lib_if, dataarg, was_secure_loaded ? plt_common_untrusting: plt_common_complete_trusting);
    } else {
        init_if_new_thread_func(dataarg);
    }

    // Then call init in other library

    init_other_object(self_ctrl, own_mop, queue, startup_flags);
}
