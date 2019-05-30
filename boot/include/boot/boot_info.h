/*-
 * Copyright (c) 2016 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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

#ifndef _BOOT_INFO_H_
#define _BOOT_INFO_H_

// FIXME: This header seems to include types needed by the OS and init. Init seems to be in the wrong directory which
// FIXME: causes all the messyness with directories.

#include "cheric.h"
#include "nano/nanokernel.h"
#include "../../../cherios/kernel/include/sched.h"
/*
 * Information populated by boot-loader, and given to the kernel via a
 * pointer in cherios main.
 */
typedef struct boot_info {
	/* These are all physical addresses */
	size_t 		nano_begin;
	size_t 		nano_end;
	size_t		kernel_begin;
	size_t 		kernel_end;
	size_t 		init_begin;
	size_t 		init_end;

	size_t		init_entry;
	size_t 		init_tls_base;
} boot_info_t;

#define MOP_SEALING_TYPE (0x666)
#define PROC_SEALING_TYPE (0x777)

typedef struct memmgt_init_t {
	nano_kernel_if_t* nano_if;
	capability nano_default_cap;
	capability mop_sealing_cap;

	capability base_mop;
	size_t 	   mop_signal_flag;
} memmgt_init_t;

typedef struct procman_init_t {
	cap_pair pool_from_init;
	nano_kernel_if_t* nano_if;
	capability nano_default_cap;
	capability sealer;
} procman_init_t;

/* Information copied from the boot_info by the kernel, and given to
 * the init activation.
 */
typedef struct init_info {
	nano_kernel_if_t* nano_if;		/* the nano kernels interface */
	capability nano_default_cap;	/* default capability for the nano kernel */
    size_t kernel_size;             /* The physical space occupied by the kernel/init activations */

	capability uart_cap;
    size_t uart_page;

	capability mop_sealing_cap;
	capability top_sealing_cap;

	sched_idle_init_t idle_init;
} init_info_t;

#endif /* _BOOT_INFO_H_ */
