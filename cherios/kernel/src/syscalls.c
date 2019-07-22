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

int in_bench = 0;

DECLARE_WITH_CD(uint64_t, kernel_syscall_bench_start(void));
__used uint64_t kernel_syscall_bench_start(void) {
	// disable all interrupts on this core
	kernel_interrupts_off();
	// start a timer
	in_bench = 1;
	return get_high_res_time(cp0_get_cpuid());
}

DECLARE_WITH_CD(uint64_t, kernel_syscall_bench_end(void));
__used uint64_t kernel_syscall_bench_end(void) {
	// finish time
	uint64_t time = get_high_res_time(cp0_get_cpuid());
	// enable interrupts again

	in_bench = 0;
	kernel_interrupts_on();
	return time;
}

DECLARE_WITH_CD(void, kernel_syscall_info_epoch(void));
__used void kernel_syscall_info_epoch(void) {
    // a bit racey but its only for debug
#if (K_DEBUG)
    FOR_EACH_ACT(act) {
            act->had_time_epoch = 0;
    }}
#endif
}

DECLARE_WITH_CD(act_control_kt, kernel_syscall_actlist_first(void));
__used act_control_kt kernel_syscall_actlist_first(void) {
	return (act_control_kt)act_create_sealed_ctrl_ref(act_list_start);
}

DECLARE_WITH_CD(act_control_kt, kernel_syscall_actlist_next(act_control_kt act));
__used act_control_kt kernel_syscall_actlist_next(act_control_kt act) {
	act_control_t* ctrl = act_unseal_ctrl_ref(act);
    act_t* next = ctrl->list_next;
	return next ? (act_control_kt)act_create_sealed_ctrl_ref(next) : NULL;
}

#if (!K_DEBUG)
user_stats_t dummy_user_stats;
#endif

DECLARE_WITH_CD(user_stats_t*, kernel_syscall_act_user_info_ref(act_control_kt act));
__used user_stats_t* kernel_syscall_act_user_info_ref(__unused act_control_kt act) {
	return cheri_setbounds_exact(
#if (K_DEBUG)
	                             &(act_unseal_ctrl_ref(act)->user_stats)
#else
	                             &dummy_user_stats
#endif
                                 , sizeof(user_stats_t));
}

DECLARE_WITH_CD(void, kernel_syscall_act_info(act_control_kt act, act_info_t* info));
__used void kernel_syscall_act_info(act_control_kt act, act_info_t* info) {
	act_control_t* ctrl = act_unseal_ctrl_ref(act);
	info->name = ctrl->name;
	info->sched_status = ctrl->sched_status;
	info->status = ctrl->status;
    info->cpu = ctrl->pool_id;

#if (K_DEBUG)
    info->commit_faults = ctrl->commit_faults;
	info->sent_n = ctrl->sent_n;
	info->received_n = ctrl->recv_n;
	info->switches = ctrl->switches;
	info->had_time = ctrl->had_time;
    info->had_time_epoch = ctrl->had_time_epoch;

	queue_t* q = ctrl->msg_queue;

	info->queue_fill = *q->header.end - q->header.start;
#define COPY_STAT(item, ...) info->item = ctrl->item;
    STAT_DEBUG_LIST(COPY_STAT)

    info->user_stats = ctrl->user_stats;
#endif
}

DECLARE_WITH_CD(void, kernel_syscall_dump_tlb(void));
__used void kernel_syscall_dump_tlb(void) {
	kernel_dump_tlb();
}

DECLARE_WITH_CD(size_t, kernel_syscall_provide_sync(res_t res));
DECLARE_WITH_CD(void, kernel_syscall_next_sync(void));
__used void kernel_syscall_next_sync(void) {
	alloc_new_indir(CALLER);
}

DECLARE_WITH_CD(void, kernel_syscall_change_priority(act_control_kt ctrl, enum sched_prio priority));
__used void kernel_syscall_change_priority(act_control_kt ctrl, enum sched_prio priority) {
	if(priority > PRIO_HIGH) return;
	act_control_t* control = act_unseal_ctrl_ref(ctrl);
	sched_change_prio((act_t*)control, priority);
}

DECLARE_WITH_CD(void, kernel_sleep(register_t timeout));
__used void kernel_sleep(register_t timeout) {
	if(timeout != 0) {
		sched_block_until_event(NULL, NULL, sched_runnable, timeout, 0);
	} else {
		sched_reschedule(NULL, 0);
	}
}

DECLARE_WITH_CD(register_t , kernel_syscall_now(void));
__used register_t kernel_syscall_now(void) {
	// FIXME: Will break if core is moved. Currently core is never moved so this is fine.
	return get_high_res_time(cp0_get_cpuid());
}

DECLARE_WITH_CD(void, kernel_syscall_vmem_notify(act_kt waiter, int suggest_switch));
__used void kernel_syscall_vmem_notify(act_kt waiter, int suggest_switch) {
	act_t* target = act_unseal_ref(waiter);
	act_t* caller = sched_get_current_act();
	if(caller != memgt_ref) {
		kernel_panic("Only memmgt should be calling this\n");
	}
	sched_receive_event(target, sched_wait_commit);
	if(suggest_switch) {
		sched_reschedule(target, 0);
	}
}

DECLARE_WITH_CD(void, kernel_wait(void));
__used void kernel_wait(void) {
	//TODO it might be nice for users to suggest next, i.e. they batch a few sends then call wait for their recipient
    sched_block_until_event(NULL, NULL, sched_waiting, 0, 0);
}

DECLARE_WITH_CD(act_control_t *, kernel_syscall_act_register(reg_frame_t *frame, char *name, queue_t *queue, res_t res, uint8_t cpu_hint));
__used act_control_t * kernel_syscall_act_register(reg_frame_t *frame, char *name, queue_t *queue, res_t res, uint8_t cpu_hint) {
	return act_register_create(frame, queue, name, status_alive, NULL, res, cpu_hint);
}

DECLARE_WITH_CD(act_t *, kernel_syscall_act_ctrl_get_ref(act_control_t* ctrl));
__used act_t * kernel_syscall_act_ctrl_get_ref(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_get_sealed_ref_from_ctrl(ctrl);
}

DECLARE_WITH_CD(status_e, kernel_syscall_act_ctrl_get_status(act_control_t* ctrl));
__used status_e kernel_syscall_act_ctrl_get_status(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_get_status(ctrl);
}

DECLARE_WITH_CD(sched_status_e, kernel_syscall_act_ctrl_get_sched_status(act_control_t* ctrl));
__used sched_status_e kernel_syscall_act_ctrl_get_sched_status(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return ctrl->sched_status;
}

DECLARE_WITH_CD(int, kernel_syscall_act_revoke(act_control_t* ctrl));
__used int kernel_syscall_act_revoke(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_revoke(ctrl);
}

DECLARE_WITH_CD(int, kernel_syscall_act_terminate(act_control_t* ctrl));
__used int kernel_syscall_act_terminate(act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return act_terminate(ctrl);
}

DECLARE_WITH_CD(void, kernel_syscall_puts(char *msg));
__used void kernel_syscall_puts(char *msg) {
	#ifndef __LITE__
	kernel_printf(KGRN"%s" KREG KRST, msg);
    #else
	(void)msg;
	#endif
}

DECLARE_WITH_CD(void, kernel_syscall_panic_proxy(act_t* act) __dead2);
__used void kernel_syscall_panic_proxy(act_t* act) { //fixme: temporary
	// Turn of interrupts makes the panic print not get screwed up
	cp0_status_ie_disable();

	kernel_printf("Activation %s has called panic\n", sched_get_current_act()->name);

    if(act != NULL) {
        if(cheri_gettype(act) == act_ref_type)
            act = act_unseal_ref(act);
        kernel_printf("Panic proxies to %s\n", act->name);
		regdump(-1, act);
    } else {
		kernel_dump_tlb();
		backtrace(cheri_getreg(11),cheri_getpcc(),cheri_getidc(),cheri_getreg(17),cheri_getreg(18));
	}
	kernel_freeze();
}

DECLARE_WITH_CD(void, kernel_syscall_panic(void) __dead2);
__used void kernel_syscall_panic(void) {
	kernel_syscall_panic_proxy(NULL);
}

DECLARE_WITH_CD(int, kernel_syscall_interrupt_register(int number, act_control_t* ctrl, register_t v0, register_t arg, capability carg));
__used int kernel_syscall_interrupt_register(int number, act_control_t* ctrl, register_t v0, register_t arg, capability carg) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return kernel_interrupt_register(number, ctrl, v0, arg, carg);
}

DECLARE_WITH_CD(int, kernel_syscall_interrupt_enable(int number, act_control_t* ctrl));
__used int kernel_syscall_interrupt_enable(int number, act_control_t* ctrl) {
	ctrl = act_unseal_ctrl_ref(ctrl);
	return kernel_interrupt_enable(number, ctrl);
}

DECLARE_WITH_CD(void, kernel_syscall_shutdown(shutdown_t mode));
__used void kernel_syscall_shutdown(shutdown_t mode) {
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
__used void kernel_syscall_register_act_event_registrar(act_t* act) {
	static int once = 0;
	if(!once) {
		once = 1;
		act = act_unseal_ref(act);
		act_set_event_ref(act);
	}
}

DECLARE_WITH_CD(const char*, kernel_syscall_get_name(act_t * act));
__used const char* kernel_syscall_get_name(act_t * act) {
    act = act_unseal_ref(act);
    const char* name = act->name;
    name = cheri_setbounds(name, sizeof(act->name));
    name = cheri_andperm(name, CHERI_PERM_LOAD);
    return name;
}

DECLARE_WITH_CD(act_notify_kt, kernel_syscall_act_ctrl_get_notify_ref(act_control_kt ctrl));
__used act_notify_kt kernel_syscall_act_ctrl_get_notify_ref(act_control_kt ctrl) {
	return act_seal_for_call(act_unseal_callable((act_t*)ctrl, ctrl_ref_sealer), notify_ref_sealer);
}

DECLARE_WITH_CD(register_t, kernel_syscall_cond_wait(int notify_on_message, register_t timeout));
__used register_t kernel_syscall_cond_wait(int notify_on_message, register_t timeout) {
    sched_status_e events = sched_wait_notify;
    if(notify_on_message) events |= sched_waiting;
    return sched_block_until_event(NULL, NULL, events, timeout, 0);
}

DECLARE_WITH_CD(void, kernel_syscall_cond_cancel(void));
__used void kernel_syscall_cond_cancel(void) {
	sched_get_current_act()->early_notify = 0;
}

DECLARE_WITH_CD(void, kernel_syscall_cond_notify(act_t* act));
__used void kernel_syscall_cond_notify(act_t* act) {
	act = act_unseal_callable(act, notify_ref_sealer);
    sched_receive_event(act, sched_wait_notify);
}

DECLARE_WITH_CD (void, kernel_message_send(capability c3, capability c4, capability c5, capability c6,
        register_t a0, register_t a1, register_t a2, register_t a3,
        act_t* target_activation, ccall_selector_t selector, register_t v0, ret_t* ret));
__used ret_t* kernel_message_send_ret(capability c3, capability c4, capability c5, capability c6,
                             register_t a0, register_t a1, register_t a2, register_t a3,
                             act_t* target_activation, ccall_selector_t selector, register_t v0);



DECLARE_WITH_CD(void, kernel_syscall_hang_debug(void));
__used void kernel_syscall_hang_debug(void) {
	dump_sched();
}

DECLARE_WITH_CD(int, kernel_message_reply(capability c3, register_t v0, register_t v1, act_t* caller, capability sync_token, int hint_switch));
DECLARE_WITH_CD(void, kernel_fastpath_wait(capability c3, register_t v0, register_t v1, act_reply_kt reply_token, int64_t timeout, int notify_is_timeout));

#define SET_IF(call, ...)\
kernel_if -> call = cheri_seal((capability)(&(__cross_domain_kernel_ ## call)), ctrl_ref_sealer);

void setup_syscall_interface(kernel_if_t* kernel_if) {
    SYS_CALL_LIST(SET_IF,)
}