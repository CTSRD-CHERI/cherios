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

#include "sys/types.h"
#include "klib.h"
#include "syscalls.h"


/*
 * These functions abstract the syscall register convention
 */

/* TODO to avoid work I will use an activation reference plus a c0 trampoline for most of these
 * TODO however, the intention is that the sealed capability should obey the principle of least privilige.
 * TODO that is, if we want to sleep it should contain just enough to do a context switch
 * TODO this works nicely when we just want, for example, to get a field from a struct
 */
void kernel_syscall_sleep(int time);
void kernel_syscall_sleep(int time) {
	if(time != 0) {
		KERNEL_ERROR("sleep >0 not implemented");
	} else {
		sched_reschedule(NULL, sched_runnable, 0);
	}
}

void kernel_syscall_wait(void);
void kernel_syscall_wait(void) {
	//TODO it might be nice for users to suggest next, i.e. they batch a few sends then call wait for their recipient
	sched_block_until_msg(kernel_curr_act, NULL);
}

act_control_t * kernel_act_register(reg_frame_t *frame, char *name, queue_t *queue);
act_control_t * kernel_act_register(reg_frame_t *frame, char *name, queue_t *queue) {
	return act_register_create(frame, queue, name, status_alive, NULL);
}

act_t * kernel_act_ctrl_get_ref(void);
act_t * kernel_act_ctrl_get_ref(void) {
	act_control_t * ctrl = (act_control_t *)get_idc();
	return act_get_sealed_ref_from_ctrl(ctrl);
}

status_e kernel_act_ctrl_get_status(void);
status_e kernel_act_ctrl_get_status(void) {
	act_control_t * ctrl = (act_control_t *)get_idc();
	return act_get_status(ctrl);
}

int kernel_act_revoke(void);
int kernel_act_revoke(void) {
	act_control_t * ctrl = (act_control_t *)get_idc();
	return act_revoke(ctrl);
}

int kernel_act_terminate(void);
int kernel_act_terminate(void) {
	act_control_t * ctrl = (act_control_t *)get_idc();
	return act_terminate(ctrl);
}

void kernel_syscall_puts(char *msg);
void kernel_syscall_puts(char *msg) {
	#ifndef __LITE__
	kernel_printf(KGRN"%s" KREG KRST, msg);
	#else
	kernel_puts(msg);
	#endif
}

void kernel_syscall_panic(void) __dead2;
void kernel_syscall_panic(void) { //fixme: temporary
	regdump(-1);
	kernel_freeze();
}

int kernel_syscall_interrupt_register(int number);
int kernel_syscall_interrupt_register(int number) {
	return kernel_interrupt_register(number, (act_control_t *)get_idc());
}

int kernel_syscall_interrupt_enable(int number);
int kernel_syscall_interrupt_enable(int number) {
	return kernel_interrupt_enable(number, (act_control_t *)get_idc());
}

int kernel_syscall_gc(capability p, capability pool);
int kernel_syscall_gc(capability p, capability pool) {
	return try_gc(p , pool);
}

DECLARE_AND_DEFINE_TRAMPOLINE(kernel_syscall_sleep)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_syscall_wait)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_act_register)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_act_ctrl_get_ref)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_act_ctrl_get_status)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_act_revoke)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_act_terminate)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_syscall_puts)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_syscall_panic)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_syscall_interrupt_register)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_syscall_interrupt_enable)
DECLARE_AND_DEFINE_TRAMPOLINE(kernel_syscall_gc)

void setup_syscall_interface(kernel_if_t* kernel_if) {

	kernel_if->sleep = kernel_seal(kernel_syscall_sleep_get_trampoline(), act_ctrl_ref_type);
	kernel_if->wait = kernel_seal(kernel_syscall_wait_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_act_register = kernel_seal(kernel_act_register_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_act_ctrl_get_ref = kernel_seal(kernel_act_ctrl_get_ref_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_act_ctrl_get_status = kernel_seal(kernel_act_ctrl_get_status_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_act_revoke = kernel_seal(kernel_act_revoke_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_act_terminate = kernel_seal(kernel_act_terminate_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_puts = kernel_seal(kernel_syscall_puts_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_panic = kernel_seal(kernel_syscall_panic_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_interrupt_register = kernel_seal(kernel_syscall_interrupt_register_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_interrupt_enable = kernel_seal(kernel_syscall_interrupt_enable_get_trampoline(), act_ctrl_ref_type);
	kernel_if->syscall_gc = kernel_seal(kernel_syscall_gc_get_trampoline(), act_ctrl_ref_type);
}