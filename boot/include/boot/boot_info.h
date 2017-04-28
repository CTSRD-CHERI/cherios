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

#include "cheric.h"
#include "nanokernel.h"
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
} boot_info_t;

typedef struct memmgt_init_t {
	res_t reservation;
	nano_kernel_if_t* nano_if;
	capability nano_default_cap;
} memmgt_init_t;

/* Information copied from the boot_info by the kernel, and given to
 * the init activation.
 */
typedef struct init_info {
	res_t 		free_mem;			/* a reservation for free memory */
	nano_kernel_if_t* nano_if;		/* the nano kernels interface */
	capability nano_default_cap;	/* default capability for the nano kernel */
} init_info_t;

#endif /* _BOOT_INFO_H_ */
