/*-
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2017 Lawrence Esswood
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

#include "sys/types.h"
#include "cheric.h"
#include "object.h"
#include "namespace.h"
#include "queue.h"
#include "assert.h"
#include "syscalls.h"
#include "thread.h"
#include "mman.h"

#if !(LIGHTWEIGHT_OBJECT)
#ifndef LIB_EARLY

link_session_t own_link_session;

#endif //
#endif // !LIGHTWEIGHT

void libuser_init(act_control_kt self_ctrl,
				  act_kt ns_ref,
				  kernel_if_t* kernel_if_c,
				  queue_t * queue,
				  capability proc,
				  mop_t mop,
				  tres_t cds_res,
				  startup_flags_e flags) {
#if !(LIGHTWEIGHT_OBJECT)
	proc_handle = proc;
#else
	(void)proc;
#endif
	mmap_set_mop(mop);
	namespace_init(ns_ref);
	object_init(self_ctrl, queue, kernel_if_c, cds_res, flags, 1);
#if !(LIGHTWEIGHT_OBJECT)
#ifndef LIB_EARLY
    auto_dylink_new_process(&own_link_session, NULL);
#endif
#endif // !LIGHTWEIGHT
}