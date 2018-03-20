/*-
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

#include "mips.h"
#include "cheric.h"
#include "cp0.h"
#include "object.h"
#include "string.h"
#include "uart.h"
#include "assert.h"
#include "stdio.h"
#include "queue.h"
#include "types.h"
#include "syscalls.h"
#include "init.h"
#include "namespace.h"
#include "elf.h"

static void * ns_ref = NULL;
static void * ns_id  = NULL;

static int act_alive(capability ctrl) {
	if(!ctrl) {
		return 0;
	}

	status_e ret = syscall_act_ctrl_get_status(ctrl);

	if(ret == status_terminated) {
		return 0;
	}
	return 1;
}

int acts_alive(init_elem_t * init_list, size_t  init_list_len) {
	int nb = 0;
	for(size_t i=0; i<init_list_len; i++) {
		init_elem_t * be = init_list + i;
		if((!be->daemon) && act_alive(be->ctrl)) {
            sched_status_e sched = syscall_act_ctrl_get_sched_status(be->ctrl);
            printf("%s is alive (%d)\n", be->name, sched);
			nb++;
            // break;
		}
	}
	return nb;
}

int num_registered_modules(void) {
	return namespace_get_num_services();
}