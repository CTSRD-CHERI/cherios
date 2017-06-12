/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "lib.h"
#include "malloc_heap.h"
#include "../../boot/include/boot/boot_info.h"
#include "thread.h"

extern void msg_entry;
void (*msg_methods[]) = {__mmap, __munmap, commit_vmem};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, ctor_null, dtor_null};
size_t ctrl_methods_nb = countof(ctrl_methods);

ALLOCATE_PLT_NANO

void register_ns(void * ns_ref) {
	namespace_init(ns_ref);
	int ret = namespace_register(namespace_num_memmgt, act_self_ref);
	if(ret!=0) {
		printf(KRED"memmgt: Register failed %d\n", ret);
	}
}

static void worker_start(register_t arg, capability carg) {

	memmgt_init_t* mem_init = (memmgt_init_t*)carg;

	int ret = namespace_register(namespace_num_memmgt, act_self_ref);
	if(ret!=0) {
		printf(KRED"memmgt: Register failed %d\n", ret);
	}

	minit();

	/* init release mecanism */
	release_init();

	syscall_puts("memmgt: Going into daemon mode\n");

	/* This thread handles everything else */
	msg_enable = 1; /* Go in waiting state instead of exiting */
}

int main(memmgt_init_t* mem_init) {
	/* So we can call nano kernel functions. This would normally be done by the linker */
	init_nano_kernel_if_t(mem_init->nano_if, mem_init->nano_default_cap);

	printf("spawning worker\n");

	/* Virtual memory fails cannot occur in this activation as exception.c relies on us answering messages during
	 * TLB miss. In order to fix this we create a worker thread that is allowed to fail. We then go into message
 	 * receiving mode immediately to handle TLB misses . */

	thread_new("memgt_worker", 0, mem_init, &worker_start);

	/* This thread handles only commit_vmem */
	msg_enable = 1;
	return 0;
}
