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

typedef struct id_s {
	register_t * buf;
	u64 sum;
} id_t;

static void sum(void) {
	static int i=0x1800;
	//id_t * id = get_curr_cookie();
	//id->sum += id->buf[0];
	if(!(++i&0x1FFF)) {
		printf("T1B: %x\n", i);
		__asm("li $0, 0x1337");
		//printf("T1B: %x %ld\n", i, id->sum);
	}
	return;
}

static void * setup(void * buf) {
	id_t * object = malloc(sizeof(id_t));
	assert(object != NULL);
	object->buf = buf;
	object->sum = 0;
	return act_seal_id(object);
}

extern void msg_entry;
void (*msg_methods[]) = {setup, sum};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

int main(void)
{
	syscall_puts("Test1B Hello world\n");

	/* Register ourself to the kernel as being the UART module */
	int ret = namespace_register(12, act_self_ref, act_self_id);
	if(ret!=0) {
		printf("Test1B: register failed\n");
		return -1;
	}

	msg_enable = 1; /* Go in waiting state instead of exiting */
	return 0;
}
