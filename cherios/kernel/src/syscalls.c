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

/*
 * These functions are those that are available by dynamic linking with the kernel
 */

#define DECLARE_WITH_CD(A, B) A B; A __cross_domain_## B; A __cross_domain_trusted_## B

DECLARE_WITH_CD(void, kernel_sleep(int time));
void kernel_sleep(int time) {
	if(time != 0) {
		KERNEL_ERROR("sleep >0 not implemented");
	} else {
		sched_reschedule(NULL, 0);
	}
}

DECLARE_WITH_CD(void, kernel_wait(void));
void kernel_wait(void) {
	//TODO it might be nice for users to suggest next, i.e. they batch a few sends then call wait for their recipient
	sched_block_until_msg(NULL, NULL);
}

DECLARE_WITH_CD(act_control_t *, kernel_syscall_act_register(reg_frame_t *frame, char *name, queue_t *queue, res_t res, uint8_t cpu_hint));
act_control_t * kernel_syscall_act_register(reg_frame_t *frame, char *name, queue_t *queue, res_t res, uint8_t cpu_hint) {
	return act_register_create(frame, queue, name, status_alive, NULL, res, cpu_hint);
}

DECLARE_WITH_CD(act_t *, kernel_syscall_act_ctrl_get_ref(act_control_t* ctrl));
act_t * kernel_syscall_act_ctrl_get_ref(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_get_sealed_ref_from_ctrl(ctrl);
}

DECLARE_WITH_CD(status_e, kernel_syscall_act_ctrl_get_status(act_control_t* ctrl));
status_e kernel_syscall_act_ctrl_get_status(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_get_status(ctrl);
}

DECLARE_WITH_CD(sched_status_e, kernel_syscall_act_ctrl_get_sched_status(act_control_t* ctrl));
sched_status_e kernel_syscall_act_ctrl_get_sched_status(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return ctrl->sched_status;
}

DECLARE_WITH_CD(int, kernel_syscall_act_revoke(act_control_t* ctrl));
int kernel_syscall_act_revoke(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_revoke(ctrl);
}

DECLARE_WITH_CD(int, kernel_syscall_act_terminate(act_control_t* ctrl));
int kernel_syscall_act_terminate(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_terminate(ctrl);
}

DECLARE_WITH_CD(void, kernel_syscall_puts(char *msg));
void kernel_syscall_puts(char *msg) {
	#ifndef __LITE__
	kernel_printf(KGRN"%s" KREG KRST, msg);
	#else
	kernel_puts(msg);
	#endif
}

DECLARE_WITH_CD(void, kernel_syscall_panic_proxy(act_t* act) __dead2);
void kernel_syscall_panic_proxy(act_t* act) { //fixme: temporary
    if(act != NULL) {
        if(cheri_gettype(act) == act_ref_type)
            act = act_unseal_ref(act);
        kernel_printf("Panic proxies to %s\n", act->name);
    }
	regdump(-1, act);
	kernel_freeze();
}

DECLARE_WITH_CD(void, kernel_syscall_panic(void) __dead2);
void kernel_syscall_panic(void) {
    return kernel_syscall_panic_proxy(NULL);
}

DECLARE_WITH_CD(int, kernel_syscall_interrupt_register(int number, act_control_t* ctrl, register_t v0, register_t arg, capability carg));
int kernel_syscall_interrupt_register(int number, act_control_t* ctrl, register_t v0, register_t arg, capability carg) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return kernel_interrupt_register(number, ctrl, v0, arg, carg);
}

DECLARE_WITH_CD(int, kernel_syscall_interrupt_enable(int number, act_control_t* ctrl));
int kernel_syscall_interrupt_enable(int number, act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return kernel_interrupt_enable(number, ctrl);
}

DECLARE_WITH_CD(void, kernel_syscall_shutdown(shutdown_t mode));
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

DECLARE_WITH_CD(void, kernel_syscall_register_act_event_registrar(act_t* act));
void kernel_syscall_register_act_event_registrar(act_t* act) {
	static int once = 0;
	if(!once) {
		once = 1;
		act = act_unseal_ref(act);
		act_set_event_ref(act);
	}
}

DECLARE_WITH_CD(const char*, kernel_syscall_get_name(act_t * act));
const char* kernel_syscall_get_name(act_t * act) {
    act = act_unseal_ref(act);
    const char* name = act->name;
    name = cheri_setbounds(name, sizeof(act->name));
    name = cheri_andperm(name, CHERI_PERM_LOAD);
    return name;
}

DECLARE_WITH_CD(act_notify_kt, kernel_syscall_act_ctrl_get_notify_ref(act_control_kt ctrl));
act_notify_kt kernel_syscall_act_ctrl_get_notify_ref(act_control_kt ctrl) {
	return act_seal_for_call(act_unseal_callable((act_t*)ctrl, ctrl_ref_sealer), notify_ref_sealer);
}

DECLARE_WITH_CD(void, kernel_syscall_cond_wait(void));
void kernel_syscall_cond_wait(void) {
	sched_wait_for_notify(NULL, NULL);
}

DECLARE_WITH_CD(void, kernel_syscall_cond_notify(act_t* act));
void kernel_syscall_cond_notify(act_t* act) {
	act = act_unseal_callable(act, notify_ref_sealer);
	sched_receives_notify(act);
}

DECLARE_WITH_CD (void, kernel_message_send(capability c3, capability c4, capability c5, capability c6,
        register_t a0, register_t a1, register_t a2, register_t a3,
        act_t* target_activation, ccall_selector_t selector, register_t v0, ret_t* ret));
void kernel_message_send_ret(capability c3, capability c4, capability c5, capability c6,
                             register_t a0, register_t a1, register_t a2, register_t a3,
                             act_t* target_activation, ccall_selector_t selector, register_t v0, ret_t* ret);



DECLARE_WITH_CD(int, kernel_message_reply(capability c3, register_t v0, register_t v1, act_t* caller, capability sync_token));

#define SET_IF(call, ...)\
kernel_if -> call = cheri_seal((capability)(&(__cross_domain_kernel_ ## call)), ctrl_ref_sealer);

void setup_syscall_interface(kernel_if_t* kernel_if) {
    SYS_CALL_LIST(SET_IF,)
}