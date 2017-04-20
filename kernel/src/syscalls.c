/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "klib.h"
#include "syscalls.h"

DEFINE_ENUM_CASE(syscalls_t, SYS_CALL_LIST)

/*
 * These functions abstract the syscall register convention
 */
static void syscall_sleep(void) {
	int time = kernel_exception_framep_ptr->mf_a0;
	if(time != 0) {
		KERNEL_ERROR("sleep >0 not implemented");
	} else {
		sched_reschedule(0);
	}
}

static void syscall_wait(void) {
	//TODO it might be nice for users to suggest next, i.e. they batch a few sends then call wait for their recipient
	act_wait(kernel_curr_act, NULL);
}

static void syscall_act_register(void) {
	reg_frame_t * frame = kernel_exception_framep_ptr->cf_c3;
	char * name = kernel_exception_framep_ptr->cf_c4;
	queue_t * queue = kernel_exception_framep_ptr->cf_c5;
	register_t a0 = kernel_exception_framep_ptr->mf_a0;
	kernel_exception_framep_ptr->cf_c3 = act_register(frame, queue, name, a0, status_alive);
}

static void syscall_act_ctrl_get_ref(void) {
	kernel_exception_framep_ptr->cf_c3 = act_get_sealed_ref_from_ctrl(kernel_exception_framep_ptr->cf_c3);
}

static void syscall_act_ctrl_get_id(void) {
	kernel_exception_framep_ptr->cf_c3 = act_get_id(kernel_exception_framep_ptr->cf_c3);
}

static void syscall_act_ctrl_get_status(void) {
	kernel_exception_framep_ptr->mf_v0 = act_get_status(kernel_exception_framep_ptr->cf_c3);
}

static void syscall_act_revoke(void) {
	kernel_exception_framep_ptr->mf_v0 = act_revoke(kernel_exception_framep_ptr->cf_c3);
}

static void syscall_act_terminate(void) {
	int ret = act_terminate(kernel_exception_framep_ptr->cf_c3);
	if(ret != 1) {
		kernel_exception_framep_ptr->mf_v0 = ret;
	}
}

static void syscall_act_seal_identifier(void) {
	kernel_exception_framep_ptr->cf_c3 = act_seal_identifier(kernel_exception_framep_ptr->cf_c3);
}

static void syscall_puts() {
	capability msg = kernel_exception_framep_ptr->cf_c3;
	#ifndef __LITE__
	kernel_printf(KGRN"%s" KREG KRST, msg);
	#else
	kernel_puts(msg);
	#endif
}

static void syscall_panic(void) __dead2;
static void syscall_panic(void) { //fixme: temporary
	regdump(-1);
	kernel_freeze();
}

static void syscall_interrupt_register(void) {
	kernel_exception_framep_ptr->mf_v0 =
		kernel_interrupt_register(kernel_exception_framep_ptr->mf_a0);
}

static void syscall_interrupt_enable(void) {
	kernel_exception_framep_ptr->mf_v0 =
		kernel_interrupt_enable(kernel_exception_framep_ptr->mf_a0);
}

static void syscall_gc(void) {
	kernel_exception_framep_ptr->mf_v0 =
	  try_gc(kernel_exception_framep_ptr->cf_c3,
	         kernel_exception_framep_ptr->cf_c4);
}

//FIXME should send message not handle itself

/*
 * Syscall demux
 */
void kernel_exception_syscall(void)
{
	syscalls_t sysn = (syscalls_t)kernel_exception_framep_ptr->mf_v0;
	KERNEL_TRACE("exception", "Syscall %s", enum_syscalls_t_tostring(sysn));
	act_t * kca = kernel_curr_act;
	switch(sysn) {
		case SLEEP:
			syscall_sleep();
			break;
		case WAIT:
			syscall_wait();
			break;
		case ACT_REGISTER:
			syscall_act_register();
			break;
		case ACT_CTRL_GET_REF:
			syscall_act_ctrl_get_ref();
			break;
		case ACT_CTRL_GET_ID:
			syscall_act_ctrl_get_id();
			break;
		case ACT_CTRL_GET_STATUS:
			syscall_act_ctrl_get_status();
			break;
		case ACT_REVOKE:
			syscall_act_revoke();
			break;
		case ACT_TERMINATE:
			syscall_act_terminate();
			break;
		case ACT_SEAL_IDENTIFIER:
			syscall_act_seal_identifier();
			break;
		case PUTS:
			syscall_puts();
			break;
		case PANIC:
			syscall_panic();
			break;
		case INTERRUPT_REGISTER:
			syscall_interrupt_register();
			break;
		case INTERRUPT_ENABLE:
			syscall_interrupt_enable();
			break;
		case GC:
			syscall_gc();
			break;
		default:
			KERNEL_ERROR("unknown syscall '%u'", sysn);
			kernel_freeze();
	}

	kernel_skip_instr(kca);
}
