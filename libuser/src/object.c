/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include <sockets.h>
#include "mips.h"
#include "object.h"
#include "cheric.h"
#include "assert.h"
#include "namespace.h"
#include "queue.h"
#include "syscalls.h"
#include "string.h"
#include "stdio.h"
#include "nano/nanokernel.h"
#include "mman.h"
#include "capmalloc.h"
#include "thread.h"
#include "exceptions.h"
#include "exception_cause.h"
#include "temporal.h"
#include "unistd.h"

capability int_cap;

__thread act_control_kt act_self_ctrl = NULL;
__thread act_kt act_self_ref  = NULL;
__thread act_notify_kt act_self_notify_ref = NULL;
__thread queue_t * act_self_queue = NULL;

kernel_if_t kernel_if;
int    was_secure_loaded;
found_id_t* own_found_id;

ALLOCATE_PLT_SYSCALLS
ALLOCATE_PLT_NANO

extern void memset_c(void);

#ifndef USE_SYSCALL_PUTS

#define STD_BUF_SIZE 0x100

typedef struct {
    unix_like_socket sock;
    struct requester_32 reqs;
    char buf[STD_BUF_SIZE];
} std_sock;

std_sock std_out_sock;
std_sock std_err_sock;

#endif

void object_init(act_control_kt self_ctrl, queue_t * queue, kernel_if_t* kernel_if_c, capability plt_auth) {

    act_self_ctrl = self_ctrl;

    /* This comes in null for threads other than the first - the interface is not thread local */
	if(kernel_if_c != NULL) {
        // I feel like as we use these methods on every syscall we should remove the indirection
        memcpy(&kernel_if, kernel_if_c, sizeof(kernel_if_t));
        init_nano_if_sys(&plt_common_single_domain, plt_auth); // <- this allows us to use non sys versions by calling syscall in advance for each function
        init_kernel_if_t(&kernel_if, self_ctrl, &plt_common_complete_trusting, plt_auth);
    } else {
        init_kernel_if_t_new_thread(&kernel_if, self_ctrl, &plt_common_complete_trusting, plt_auth);
    }

    int_cap = get_integer_space_cap();

	act_self_ref  = syscall_act_ctrl_get_ref(self_ctrl);
    act_self_notify_ref = syscall_act_ctrl_get_notify_ref(self_ctrl);
	act_self_queue = queue;

    sync_state = (sync_state_t){.sync_caller = NULL};

    // Tag exceptions can happen when we first use an unsafe stack. We will handle these to get a stack.
    // We can also get a length violation if we need a new one.
    register_vectored_cap_exception(&temporal_exception_handle, Tag_Violation);
    register_vectored_cap_exception(&temporal_exception_handle, Length_Violation);

    own_found_id = foundation_get_id();
    was_secure_loaded = (own_found_id != NULL);

    init_cap_malloc();

    thread_init();

    // This creates two sockets with the UART driver and sets stdout/stderr to them
#ifndef USE_SYSCALL_PUTS

    act_kt uart;

    while((uart = namespace_get_ref(namespace_num_uart)) == NULL) {
        sleep(0);
    }

    int res;

    int flags = MSG_NO_CAPS | SOCKF_WR_INLINE | SOCKF_DRB_INLINE | SOCKF_WR_INLINE | SOCKF_SOCK_INLINE;

#define MAKE_STD_SOCK(S,IPC_NO)                                                                                 \
        S.sock.write.push_writer = &S.reqs.r;                                                                   \
        socket_internal_requester_init(S.sock.write.push_writer, 32, SOCK_TYPE_PUSH, &S.sock.write_copy_buffer);                   \
            res = message_send(0,0,0,0,                                                                         \
                               socket_internal_make_read_only(S.sock.write.push_writer), NULL, NULL, NULL,      \
                               uart, SYNC_CALL, IPC_NO);                                                        \
        assert(res == 0);                                                                                       \
        socket_internal_requester_connect(S.sock.write.push_writer);                                            \
        socket_init(&S.sock, flags, S.buf, STD_BUF_SIZE, CONNECT_PUSH_WRITE);

    MAKE_STD_SOCK(std_out_sock, 2);
    MAKE_STD_SOCK(std_err_sock, 3);

    stderr = &std_err_sock.sock;
    stdout = &std_out_sock.sock;

#else
    stderr = NULL;
    stdout = NULL;
#endif

}

// Called when main exits
__attribute__((noreturn))
void object_destroy() {
    #ifndef USE_SYSCALL_PUTS
        flush(stdout);
        flush(stderr);
    #endif
    syscall_act_terminate(act_self_ctrl);
}

void ctor_null(void) {
	return;
}

void dtor_null(void) {
	return;
}