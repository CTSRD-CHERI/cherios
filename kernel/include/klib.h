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
#include "mips.h"
#include "activations.h"
#include "cdefs.h"
#include "cheric.h"
#include "colors.h"
#include "math.h"
#include "sched.h"
#include "string.h"
#include "kutils.h"

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
	#define	kernel_assert(e)
	#define KERNEL_ERROR(...)
#endif

// FIXME we need to really think about the types of IDs and REFs

/* The type of object activation references */
static const uint64_t act_ref_type = 42002;
/* The type of object identifier references */
static const uint64_t act_id_type = act_ref_type;
/* The type of object activation identifier control references */
static const uint64_t act_ctrl_ref_type = 42001;
/* The type of the synchronous sequence reply token */
static const uint64_t act_sync_type = 42000;
/* The type of object activation response references */
static const uint64_t act_sync_ref_type = 42003;
/*
 * Kernel library routines.
 */
void	kernel_skip_instr(act_t * act);
void	kernel_ccall(void);
void	kernel_creturn(void);
void	kernel_exception_syscall(void);

void	kernel_interrupts_init(int enable_timer);
void	kernel_interrupt(void);
int	kernel_interrupt_register(int number);
int	kernel_interrupt_enable(int number);

void	kernel_timer_init(void);
void	kernel_timer(void);

void	kernel_puts(const char *s);
void	kernel_panic(const char *s) __dead2;
#ifndef __LITE__
#define printf kernel_printf
int	kernel_printf(const char *fmt, ...);
int	kernel_vprintf(const char *fmt, va_list ap);
void	__kernel_assert(const char *, const char *, int, const char *) __dead2;
void	kernel_trace(const char *context, const char *fmt, ...);
void 	kernel_error(const char *file, const char *func, int line, const char *fmt, ...);
void	kernel_vtrace(const char *context, const char *fmt, va_list ap);
#endif

void	hw_reboot(void) __dead2;
void	kernel_freeze(void) __dead2;

int	try_gc(void * p, void * pool);

int	msg_push(act_t * dest, act_t * src, capability, capability);
void	msg_pop(act_t* act);
void	msg_queue_init(act_t* act, queue_t * queue);
int	msg_queue_empty(act_t* act);

void	act_init(void);
void	act_wait(act_t* act, act_t* next_hint);
act_control_t *	act_register(const reg_frame_t * frame, queue_t * queue, const char * name);
act_t *	act_get_sealed_ref_from_ctrl(act_control_t * ctrl);
capability act_get_id(act_control_t * ctrl);

int	act_get_status(act_control_t * ctrl);
int	act_revoke(act_control_t * ctrl);
int	act_terminate(act_control_t * ctrl);
capability act_seal_identifier(capability identifier);

void	regdump(int reg_num);

//FIXME this is also temporary
extern queue_t kernel_message_queues[];

#endif /* _CHERIOS_KLIB_H_ */
