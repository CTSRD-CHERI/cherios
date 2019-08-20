/*-
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2018 Lawrence Esswood
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
#include "thread.h"
#include "malta_virtio_mmio.h"
#include "misc.h"
#include "stdio.h"
#include "namespace.h"
#include "object.h"
#include "assert.h"
#include "mman.h"

void (*msg_methods[]) = {vblk_init, vblk_read, vblk_write, vblk_status, vblk_size, new_socket};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, new_session, NULL, vblk_interrupt};
size_t ctrl_methods_nb = countof(ctrl_methods);



__unused void init_net_device(__unused void* mmio_cap) {

}

int main(void) {
	printf("Virtio-blk: Hello world\n");

	session_sealer = get_type_owned_by_process();

	/* Register ourself to the kernel as being the Virtio-blk module */
	int ret = namespace_register(namespace_num_virtio, act_self_ref);
	if(ret!=0) {
		printf("Virtio-blk: register failed\n");
		return -1;
	}
	printf("Virtio-blk: register OK\n");

	printf("Virtio-blk: Going into daemon mode\n");

	handle_loop();

	assert(0 && "Should not reach here");

	while(1){}
}
