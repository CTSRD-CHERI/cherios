/*-
 * Copyright (c) 2011 Robert N. M. Watson
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

#ifndef _CHERIOS_KLIB_H_
#define	_CHERIOS_KLIB_H_

#include "mips.h"
#include "cdefs.h"
#include "stdarg.h"
#include "colors.h"
#include "cp0.h"
#include "cp2.h"
#include "cheric.h"

#define __TRACE__

#ifdef __TRACE__
	#define KERNEL_TRACE kernel_trace
	#define KERNEL_VTRACE kernel_vtrace
#else
	#define KERNEL_TRACE(...)
	#define KERNEL_VTRACE(...)
#endif
#define KERNEL_ERROR(...) kernel_error(__FILE__, __func__, __LINE__, __VA_ARGS__)

#define	kernel_assert(e)	((e) ? (void)0 : __kernel_assert(__func__, \
				__FILE__, __LINE__, #e))

/*
 * Kernel library routines.
 */
#define printf kernel_printf
int	kernel_printf(const char *fmt, ...);
int	kernel_vprintf(const char *fmt, va_list ap);
void	kernel_panic(const char *fmt, ...) __dead2;
void	__kernel_assert(const char *, const char *, int, const char *) __dead2;
void	kernel_trace(const char *context, const char *fmt, ...);
void	kernel_trace_boot(const char *fmt, ...);
void	kernel_trace_exception(const char *fmt, ...);
void	kernel_vtrace(const char *context, const char *fmt, va_list ap);
void	kernel_sleep(int unit);
void *	kernel_alloc(size_t s);
void	kernel_timer_init(void);
void	kernel_proc_init(void);
void 	kernel_error(const char *file, const char *func, int line, const char *fmt, ...);
void	uart_putchar(int c, void *arg);
void	elf_loader(const char * s);
void *	load (const char * filename, int * len);
void	kernel_freeze(void);
int	kernel_exec(register_t addr, register_t arg);
void	socket_init(void);
void	sleep(int n);
void	kernel_object_init(void);
void	kernel_methods_init(void);

void	*kernel_calloc(size_t, size_t);
void	 kernel_free(void *);
void	*kernel_malloc(size_t);
void	*kernel_realloc(void *, size_t);

/*
* Kernel constants.
*/
#define	CHERIOS_PAGESIZE	4096

/* Except struct/routines */
typedef  struct
{
	int runnable;
	int parent;
	int ccaller;
}  proc_t;
#define MAX_PROCESSES 16
int	task_create_bare(void);
void	kernel_reschedule(void);
void	kernel_skip(void);
void	kernel_skip_pid(int pid);
void	kernel_ccall(void);
void	kernel_creturn(void);
void	kernel_exception_syscall(void);
void *	_syscall_get_kernel_object(void);
void *	_syscall_get_kernel_methods(void);
void 	kernel_proc_set_pc(register_t addr, size_t process);
int object_register(int nb, int flags, void * methods, int methods_nb, void * data_cap);
void	kernel_timer(void);

extern struct mips_frame	kernel_exception_framep[];
extern struct cp2_frame		kernel_cp2_exception_framep[];
extern proc_t			kernel_procs[];
extern struct mips_frame *	kernel_exception_framep_ptr;
extern struct cp2_frame *	kernel_cp2_exception_framep_ptr;
extern int 			kernel_curr_proc;
extern int			kernel_next_proc;

#endif /* _CHERIOS_KLIB_H_ */
