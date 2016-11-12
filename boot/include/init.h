/*-
 * Copyright (c) 2016 Robert N. M. Watson
 * Copyright (c) 2016 Hadrien Barral
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

#ifndef _INIT_H_
#define _INIT_H_

#include "mips.h"
#include "cdefs.h"
#include "stdio.h"

typedef enum module_type {
	m_memmgt,
	m_namespace,
	m_uart,
	m_fs,
	m_core,
	m_user,
	m_fence
} module_t;

typedef struct init_elem_s {
	module_t     type;
	int          cond;
	const char * name;
	register_t   arg;
	int          daemon;
	int          status;
	void 	   * ctrl;
} init_elem_t;

extern char	__start_heap;
extern char	__stop_heap;

/*
 * Memory routines
 */
void	init_alloc_init(void);
void	init_alloc_enable_system(void * ctrl);
void *	init_alloc(size_t s);
void	init_free(void * p);

void	glue_memmgt(void * memmgt_ctrl, void* ns_ctrl);

int	acts_alive(init_elem_t * init_list, size_t  init_list_len);

void *	load_module(module_t type, const char * file, int arg);

void	stats_init(void);
void	stats_display(void);

int init_main(void);

#endif
