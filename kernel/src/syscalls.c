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

#include "klib.h"

/*
 * These functions abstract the syscall register convention
 */
static void syscall_puts() {
	void * msg = kernel_exception_framep_ptr->cf_c3;
	printf(KGRN"%s"KRST, msg);
}

static void syscall_act_register(void) {
	reg_frame_t * frame = kernel_exception_framep_ptr->cf_c3;
	kernel_exception_framep_ptr->cf_c3 = act_register(frame);
}

static void syscall_act_ctrl_get_ref(void) {
	kernel_exception_framep_ptr->cf_c3 = act_get_ref(kernel_exception_framep_ptr->cf_c3);
}

static void syscall_act_ctrl_get_id(void) {
	kernel_exception_framep_ptr->cf_c3 = act_get_id(kernel_exception_framep_ptr->cf_c3);
}

static void syscall_sleep(void) {
	kernel_skip();
	int time = kernel_exception_framep_ptr->mf_a0;
	if(time != 0) {
		KERNEL_ERROR("sleep >0 not implemented");
	} else {
		kernel_reschedule();
	}
}

static void syscall_panic(void) { //fixme: temporary
	kernel_freeze();
}

static void syscall_gc(void) {
	kernel_exception_framep_ptr->mf_v0 =
	  try_gc(kernel_exception_framep_ptr->cf_c3,
	         kernel_exception_framep_ptr->cf_c4);
}

/*
 * Syscall demux
 */
void kernel_exception_syscall(void)
{
	long sysn = kernel_exception_framep_ptr->mf_v0;
	KERNEL_TRACE("exception", "Syscall number %ld", sysn);
	int skip = 1;
	switch(sysn) {
		case 13:
			syscall_sleep();
			skip = 0;
			break;
		case 20:
			syscall_act_register();
			break;
		case 21:
			syscall_act_ctrl_get_ref();
			break;
		case 22:
			syscall_act_ctrl_get_id();
			break;
		case 34:
			syscall_puts();
			break;
		case 42:
			syscall_panic();
			break;
		case 66:
			syscall_gc();
			break;
		default:
			KERNEL_ERROR("unknown syscall '%d'", sysn);
			kernel_freeze();
	}

	if(skip) {
		kernel_skip();
	}
}
