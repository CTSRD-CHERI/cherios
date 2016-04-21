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
#include "lib.h"

void syscall_rand() {
	static int n = 42424242;
	n = 3*n+685;
	kernel_exception_framep_ptr->mf_v0 = (n >> 10) & 0xFF;
}

void syscall_putchar() {
	static size_t offset;
	const size_t buf_size = 0x100;
	static char buf[buf_size];
	char chr = kernel_exception_framep_ptr->mf_a0;
	kernel_assert(offset < buf_size-1);
	buf[offset++] = chr;
	if((chr == '\n') || (offset == buf_size)) {
		for(size_t i=0; i<offset; i++) {
			uart_putchar(buf[i], NULL);
		}
		offset = 0;
	}
}

void syscall_puts() {
	void * msg = kernel_cp2_exception_framep_ptr->cf_c3;
	printf(KGRN"%s"KRST"\n", msg);
}

/* not secure */
void syscall_exec(void) {
	/* return value (-1:fail, n=task nb) */

	kernel_exception_framep_ptr->mf_v0 = -1;

	/* zero everything */
	memset(kernel_exception_framep     + kernel_next_proc, 0, sizeof(struct mips_frame)    );
	memset(kernel_cp2_exception_framep + kernel_next_proc, 0, sizeof(struct cp2_frame));

	/* set pc */
	kernel_proc_set_pc(kernel_exception_framep_ptr->mf_a0, kernel_next_proc);

	/* set stack */
	size_t stack_size = 0x10000;
	void * stack = kernel_calloc(stack_size, 1);
	if(!stack) {
		return;
	}
	kernel_cp2_exception_framep[kernel_next_proc].cf_c11 = stack;
	kernel_exception_framep[kernel_next_proc].mf_sp = stack_size;

	/* set c12 */
	kernel_cp2_exception_framep[kernel_next_proc].cf_c12 =
			kernel_cp2_exception_framep[kernel_next_proc].cf_pcc;
			
	/* set c0 */
	kernel_cp2_exception_framep[kernel_next_proc].cf_c0 = kernel_cp2_exception_framep_ptr->cf_c0;

	/* set a0 */
	kernel_exception_framep[kernel_next_proc].mf_a0 = kernel_exception_framep_ptr->mf_a1;
	
	/* done, update next_proc */
	KERNEL_TRACE("exception", "Syscall 'exec' OK! addr:'0x%lx' arg:'%lX' stack:'%p'",
		kernel_exception_framep_ptr->mf_a0, kernel_exception_framep_ptr->mf_a1,
		kernel_cp2_exception_framep[kernel_next_proc].cf_c11);
	kernel_exception_framep_ptr->mf_v0 = kernel_next_proc;
	kernel_next_proc++;
}

static void syscall_malloc(void) {
	size_t s = kernel_exception_framep_ptr->mf_a0;
	void * p = kernel_malloc(s);
	kernel_cp2_exception_framep_ptr->cf_c3 = p;
}

static void syscall_free(void) {
	void * p = kernel_cp2_exception_framep_ptr->cf_c3;
	kernel_free(p);
}

static void syscall_sleep(void) {
	kernel_skip();
	int time = kernel_exception_framep_ptr->mf_a0;
	if(time == -1) {
		kernel_procs[kernel_curr_proc].runnable = 0;
	}
	kernel_reschedule();
}

static void syscall_register(void) {
	int nb = kernel_exception_framep_ptr->mf_a0;
	int flags = kernel_exception_framep_ptr->mf_a1;
	int methods_nb = kernel_exception_framep_ptr->mf_a2;
	void * methods  = kernel_cp2_exception_framep_ptr->cf_c3;
	void * data_cap = kernel_cp2_exception_framep_ptr->cf_c4;

	kernel_exception_framep_ptr->mf_v0 =
	  object_register(nb, flags, methods, methods_nb, data_cap);
}

static void syscall_get_kernel_methods(void) {
	kernel_cp2_exception_framep_ptr->cf_c3 = _syscall_get_kernel_methods();
}

static void syscall_get_kernel_object(void) {
	kernel_cp2_exception_framep_ptr->cf_c3 = _syscall_get_kernel_object();
}


void kernel_exception_syscall(void)
{
	long sysn = kernel_exception_framep_ptr->mf_v0;
	KERNEL_TRACE("exception", "Syscall number %ld", sysn);
	int skip = 1;
	switch(sysn) {
		case 5:
			syscall_register();
			break;
		case 13:
			syscall_sleep();
			skip = 0;
			break;
		case 17:
			syscall_malloc();
			break;
		case 18:
			syscall_free();
			break;
		case 20:
			syscall_exec();
			break;
		case 33:
			syscall_putchar();
			break;
		case 34:
			syscall_puts();
			break;
		case 42:
			syscall_rand();
			break;
		case 98:
			syscall_get_kernel_methods();
			break;
		case 99:
			syscall_get_kernel_object();
			break;
		default:
			KERNEL_ERROR("unknown syscall '%d'", sysn);
	}
	
	if(skip) {
		kernel_skip();
	}
}
