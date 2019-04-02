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
#ifndef CHERIOS_DYLINK_CLIENT_H
#define CHERIOS_DYLINK_CLIENT_H

#include "mman.h"
#include "thread.h"

#define DYLINK_IPC_NO_GET_IF            1
#define DYLINK_IPC_NO_GET_TABLE_SIZE    0
#define DYLINK_IPC_NO_GET               2

typedef void init_other_object_func_t(act_control_kt self_ctrl, mop_t* mop, queue_t * queue, startup_flags_e startup_flags);

void dylink(act_control_kt self_ctrl, queue_t * queue, startup_flags_e startup_flags, int first_thread,
            act_kt dylink_server, init_if_func_t* init_if_func, init_if_new_thread_func_t* init_if_new_thread_func,
            init_other_object_func_t * init_other_object);

#define DYLINK_LIB(lib, self_ctrl, queue, flags, first_thread, server) \
    dylink(self_ctrl, queue, flags, first_thread, server,  PLT_INIT_MAIN_THREAD(lib), PLT_INIT_NEW_THREAD(lib), INIT_OTHER_OBJECT(lib))


#endif //CHERIOS_DYLINK_CLIENT_H
