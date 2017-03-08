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
#include "uart.h"

void * uart_cap = NULL;

static void user_putc(char c) {
	printf(KGRN KBLD"%c"KRST, c);
}

static void user_puts(const void * s) {
	printf(KGRN KBLD"%s"KRST, s);
}

extern void msg_entry;
void (*msg_methods[]) = {user_putc, user_puts};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, ctor_null, dtor_null};
size_t ctrl_methods_nb = countof(ctrl_methods);

int main(void)
{
	syscall_puts("UART: Hello world\n");

	/* Get capability to use uart */
	uart_cap = act_get_cap();
	assert(VCAP(uart_cap, 0, VCAP_RW));

	/* Register ourself to the kernel as being the UART module */
	int ret = namespace_register(namespace_num_uart, act_self_ref);
	if(ret!=0) {
		syscall_puts("UART: register failed\n");
		return -1;
	}

	#if 0
	uart_init(); /* done during boot process */
	#endif

	syscall_puts("UART: Going into daemon mode\n");

	msg_enable = 1; /* Go in waiting state instead of exiting */
	return 0;
}
