/*-
 * Copyright (c) 2011 Robert N. M. Watson
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

#ifndef _CHERIOS_KLIB_H_
#define	_CHERIOS_KLIB_H_

#include "kernel.h"
#include "boot_info.h"
#include "stddef.h"
#include "activations.h"
#include "cdefs.h"
#include "cheric.h"
#include "colors.h"
#include "math.h"
#include "sched.h"
#include "string.h"
#include "kutils.h"
#include "syscalls.h"

#ifdef __TRACE__
	#define KERNEL_TRACE kernel_trace
	#define KERNEL_VTRACE kernel_vtrace
#else
	#define KERNEL_TRACE(...)
	#define KERNEL_VTRACE(...)
#endif

#ifndef __LITE__
	#include "stdarg.h"
	#define	kernel_assert(e)	((e) ? (void)0 : __kernel_assert(__func__, \
				__FILE__, __LINE__, #e))
	#define KERNEL_ERROR(...) kernel_error(__FILE__, __func__, __LINE__, __VA_ARGS__)
#else
	#define kernel_printf(...)
	#define	kernel_assert(e)
	#define KERNEL_ERROR(...)
	#define kernel_puts(...)
#endif

// FIXME we need to really think about the types of IDs and REFs

/* The type of object activation references */
static const uint64_t act_ref_type = 0x4202;
/* The type of object activation control references */
static const uint64_t act_ctrl_ref_type = 0x4201;
/* The type of the synchronous sequence reply tokens */
static const uint64_t act_sync_type = 0x4200;
/* The type of object activation response references */
static const uint64_t act_sync_ref_type = 0x4203;
/* The type of object activation notification references */
static const uint64_t act_notify_ref_type = 0x4204;

extern sealing_cap sync_token_sealer;
extern sealing_cap ctrl_ref_sealer;
extern sealing_cap notify_ref_sealer;
extern sealing_cap sync_ref_sealer;
extern sealing_cap ref_sealer;

static inline capability act_seal_for_call(act_t * act, sealing_cap sealer) {
	return cheri_seal(act, sealer);
}

static inline act_t* act_unseal_callable(act_t * act, sealing_cap sealer) {
	return (act_t*)cheri_unseal(act, sealer);
}

static inline act_t * act_create_sealed_ref(act_t * act) {
	return (act_t *)act_seal_for_call(act, ref_sealer);
}

static inline act_control_t * act_create_sealed_ctrl_ref(act_t * act) {
	return (act_control_t *)act_seal_for_call(act, ctrl_ref_sealer);
}

static inline act_t * act_unseal_ref(act_t * act) {
	return  (act_t *)act_unseal_callable(act, ref_sealer);
}

static inline act_control_t* act_unseal_ctrl_ref(act_t* act) {
	return (act_control_t*)act_unseal_callable(act, ctrl_ref_sealer);
}

static inline act_t * act_create_sealed_sync_ref(act_t * act) {
	return (act_t *)act_seal_for_call(act, sync_ref_sealer);
}

static inline act_t * act_unseal_sync_ref(act_t * act) {
	return  (act_t *)act_unseal_callable(act, sync_ref_sealer);
}

void kernel_platform_init(page_t* book);

extern if_req_auth_t req_auth_for_activations;
/*
 * Kernel library routines.
 */

void	kernel_ccall(void);
void	kernel_creturn(void);

void	kernel_interrupts_init(int enable_timer, uint8_t cpu_id);
void	kernel_interrupt(register_t cause, uint8_t cpu_id);
int kernel_interrupt_register(int number, act_control_t *ctrl, register_t v0, register_t arg, capability carg);
int kernel_interrupt_enable(int number, act_control_t *ctrl);

int kernel_interrupts_on(void);
int kernel_interrupts_off(void);

void	kernel_timer_init(uint8_t cpu_id);
void	kernel_timer(uint8_t cpu_id);
void 	kernel_timer_start_count(act_t* act);
void 	kernel_timer_subscribe(act_t* act, register_t timeout);
void 	kernel_timer_unsubcsribe(act_t* act);
uint64_t get_high_res_time(uint8_t cpu_id);

void	kernel_panic(const char *s) __dead2;

#ifndef __LITE__
void	kernel_puts(const char *s);
int	kernel_printf(const char *fmt, ...) __printflike(1, 2);
int	kernel_vprintf(const char *fmt, va_list ap);
void	__kernel_assert(const char *, const char *, int, const char *) __dead2;
void	kernel_trace(const char *context, const char *fmt, ...) __printflike(2, 3);
void 	kernel_error(const char *file, const char *func, int line, const char *fmt, ...) __printflike(4, 5);
void	kernel_vtrace(const char *context, const char *fmt, va_list ap);
#endif

void	hw_reboot(void) __dead2;
void	kernel_freeze(void) __dead2;

int msg_push(register_t a0, register_t a1, register_t a2, register_t a3,
             capability c3, capability c4, capability c5, capability c6,
			 register_t v0,
			 act_t * dest, act_t * src, capability sync_token);
void	msg_queue_init(act_t* act, queue_t * queue);
int	msg_queue_empty(act_t* act);

sync_indirection* alloc_new_indir(act_t* ccaller);

context_t	act_init(context_t own_context, init_info_t* info, size_t init_base, size_t init_entry, size_t init_tls_base,
					capability global_pcc);
void	act_wait(act_t* act, act_t* next_hint);
act_t * act_register(reg_frame_t *frame, queue_t *queue, const char *name,
					 status_e create_in_status, act_control_t *parent, size_t base, res_t res);
act_control_t * act_register_create(reg_frame_t *frame, queue_t *queue, const char *name,
								   status_e create_in_status, act_control_t *parent, res_t res, uint8_t cpu_hint);
act_t *	act_get_sealed_ref_from_ctrl(act_control_t * ctrl);
capability act_get_id(act_control_t * ctrl);

void act_set_event_ref(act_t* act);

status_e act_get_status(act_control_t *ctrl);
int	act_revoke(act_control_t * ctrl);
int	act_terminate(act_control_t * ctrl);
capability act_seal_identifier(capability identifier);

// Only possible while debugging
void kernel_dump_tlb(void);
void	regdump(int reg_num, act_t* act);
void backtrace(char* stack_pointer, capability return_address, capability idc, capability r17, capability c18);

void setup_syscall_interface(kernel_if_t* kernel_if);

void kernel_exception(context_t swap_to, context_t own_context);

#define CALLER (cheri_getidc())

#endif /* _CHERIOS_KLIB_H_ */
