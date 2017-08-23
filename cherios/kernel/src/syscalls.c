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

#include <syscalls.h>
#include <activations.h>
#include "sys/types.h"
#include "klib.h"
#include "syscalls.h"


//FIXME we can get rid of if c2 is always the object reference which then contains an unsealing cap
//FIXME see how the nano kernel does it

/*
 * These functions abstract the syscall register convention
 */

/* TODO to avoid work I will use an activation reference plus a c0 trampoline for most of these
 * TODO however, the intention is that the sealed capability should obey the principle of least privilige.
 * TODO that is, if we want to sleep it should contain just enough to do a context switch
 * TODO this works nicely when we just want, for example, to get a field from a struct
 */
void kernel_sleep(int time);
void kernel_sleep(int time) {
	if(time != 0) {
		KERNEL_ERROR("sleep >0 not implemented");
	} else {
		sched_reschedule(NULL, 0);
	}
}

void kernel_wait(void);
void kernel_wait(void) {
	//TODO it might be nice for users to suggest next, i.e. they batch a few sends then call wait for their recipient
	sched_block_until_msg(kernel_curr_act, NULL);
}

act_control_t * kernel_syscall_act_register(reg_frame_t *frame, char *name, queue_t *queue, res_t res);
act_control_t * kernel_syscall_act_register(reg_frame_t *frame, char *name, queue_t *queue, res_t res) {
	return act_register_create(frame, queue, name, status_alive, NULL, res);
}

act_t * kernel_syscall_act_ctrl_get_ref(act_control_t* ctrl);
act_t * kernel_syscall_act_ctrl_get_ref(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_get_sealed_ref_from_ctrl(ctrl);
}

status_e kernel_syscall_act_ctrl_get_status(act_control_t* ctrl);
status_e kernel_syscall_act_ctrl_get_status(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
    KERNEL_TRACE("get status", "Level: %ld. Cause: %lx", *ex_lvl, *ex_cause);
	return act_get_status(ctrl);
}

sched_status_e kernel_syscall_act_ctrl_get_sched_status(act_control_t* ctrl);
sched_status_e kernel_syscall_act_ctrl_get_sched_status(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return ctrl->sched_status;
}

int kernel_syscall_act_revoke(act_control_t* ctrl);
int kernel_syscall_act_revoke(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_revoke(ctrl);
}

int kernel_syscall_act_terminate(act_control_t* ctrl);
int kernel_syscall_act_terminate(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
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

void kernel_syscall_shutdown(shutdown_t mode);
void kernel_syscall_shutdown(shutdown_t mode) {
    // Mode if we want restart/shotdown etc
    switch(mode) {
        case REBOOT:
            hw_reboot();
        case SHUTDOWN:
            // FIXME
            kernel_printf("This shold shutdown. Not reboot");
            hw_reboot();
    }

}

void kernel_syscall_register_act_event_registrar(act_t* act);
void kernel_syscall_register_act_event_registrar(act_t* act) {
	static int once = 0;
	if(!once) {
		once = 1;
		act = act_unseal_ref(act);
		act_set_event_ref(act);
	}
}

extern void kernel_message_send(capability c3, capability c4, capability c5, capability c6,
                         register_t a0, register_t a1, register_t a2, register_t a3,
                         act_t* target_activation, ccall_selector_t selector, register_t v0, ret_t* ret);

extern int kernel_message_reply(capability c3, register_t v0, register_t v1, act_t* caller, capability sync_token);


#define message_send_BEFORE	\
	"daddiu $sp, $sp, -64\n"			\
	"csetoffset $c8, $c11, $sp\n"

#define message_send_AFTER	\
	"clc	$c3, $sp, 0($c11)\n"		\
	"cld $v0, $sp, 32($c11)\n"			\
	"cld $v1, $sp, 40($c11)\n"			\
	"daddiu $sp, $sp, 64\n"				\

#define SET_IF(call, ...)\
kernel_if -> call = kernel_seal((capability)(&(kernel_ ## call ## _trampoline)), act_ctrl_ref_type);


#define DADT(call) DEFINE_TRAMPOLINE_EXTRA(kernel_ ## call,,)

DEFINE_TRAMPOLINE_EXTRA(kernel_message_send, message_send_BEFORE, message_send_AFTER);
DADT(message_reply)
DADT(sleep)
DADT(wait)
DADT(syscall_act_register)
DADT(syscall_act_ctrl_get_ref)
DADT(syscall_act_ctrl_get_status)
DADT(syscall_act_ctrl_get_sched_status)
DADT(syscall_act_revoke)
DADT(syscall_act_terminate)
DADT(syscall_puts)
DADT(syscall_panic)
DADT(syscall_interrupt_register)
DADT(syscall_interrupt_enable)
DADT(syscall_shutdown)
DADT(syscall_register_act_event_registrar)

void setup_syscall_interface(kernel_if_t* kernel_if) {
    SYS_CALL_LIST(SET_IF,)
}