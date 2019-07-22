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

#include "uart.h"
#include "stdio.h"
#include "misc.h"
#include "syscalls.h"
#include "object.h"
#include "assert.h"
#include "namespace.h"
#include "sockets.h"
#include "lists.h"
#include "stdlib.h"

static void user_putc(char c) {
	printf(KGRN KBLD"%c"KRST, c);
}

static void user_puts(const void * s) {
	printf(KGRN KBLD"%s"KRST, s);
}

typedef struct f_list_item {
	fulfiller_t f;
	DLL_LINK(f_list_item);
} f_list_item;

typedef struct {
	DLL(f_list_item);
} f_list;

f_list f_list_err;
f_list f_list_out;

static f_list_item* create_f(requester_t requester) {

	f_list_item* item = (f_list_item*)malloc(sizeof(f_list_item));

	fulfiller_t f = socket_malloc_fulfiller(SOCK_TYPE_PUSH);

	item->f = f;

	assert(f != NULL);

	__unused int res = socket_fulfiller_connect(item->f, requester);

	assert_int_ex(res, ==, 0);

	return item;
}

static int create_stdout(requester_t requester) {

	f_list_item* item = create_f(requester);

	DLL_ADD_END(&f_list_out, item);

	return 0;
}

static int create_stderr(requester_t requester) {

	f_list_item* item = create_f(requester);

	DLL_ADD_END(&f_list_err, item);

	return 0;
}

extern ssize_t TRUSTED_CROSS_DOMAIN(ff)(capability arg, char* buf, uint64_t offset, uint64_t length);
__used ssize_t ff(__unused capability arg, char* buf, __unused uint64_t offset, uint64_t length) {
	for(size_t i = 0; i != length; i++) uart_putc(buf[i]);
	return length;
}

static int handle_f(f_list_item* item, enum poll_events event, int is_er) {
	if(event & POLL_OUT) {
		socket_fulfill_progress_bytes_unauthorised(item->f, SOCK_INF, F_DONT_WAIT | F_CHECK | F_PROGRESS | F_SKIP_OOB,
											   &TRUSTED_CROSS_DOMAIN(ff), NULL, 0, NULL, NULL,
											   TRUSTED_DATA, NULL);
	} else if(event & POLL_HUP) {
		socket_close_fulfiller(item->f, 0, 0);
		f_list* list = is_er ? &f_list_err : &f_list_out;
		DLL_REMOVE(list, item);
		return 1;
	} else if(event) {
		printf("Event was %x\n", event);
		assert(0);
	}
	return 0;
}

static void main_loop(void) {

	// Poll loop that loops over both lists of sockets and calls a handle function

	POLL_LOOP_START(sleep_var, event_var, 1)
		DLL_FOREACH(f_list_item, item, &f_list_out) {
			POLL_ITEM_F(event, sleep_var, event_var, item->f, POLL_OUT, 0);
			if(event && handle_f(item, event, 0)) DLL_FOREACH_RESET(f_list_item,item, &f_list_out);
		}
		DLL_FOREACH(f_list_item, item, &f_list_err) {
			POLL_ITEM_F(event, sleep_var, event_var, item->f, POLL_OUT, 0);
			if(event && handle_f(item, event, 1)) DLL_FOREACH_RESET(f_list_item,item, &f_list_err);
		}
	POLL_LOOP_END(sleep_var, event_var, 1, 0)
}

void (*msg_methods[]) = {user_putc, user_puts, create_stdout, create_stderr};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, ctor_null, dtor_null};
size_t ctrl_methods_nb = countof(ctrl_methods);

int main(capability uart_cap)
{
	syscall_puts("UART: Hello world\n");

	// Wait for libsocket
	while(namespace_get_ref(namespace_num_lib_socket) == NULL) {
		sleep(MS_TO_CLOCK(10));
	}

	dylink_sockets(act_self_ctrl, act_self_queue, default_flags, 1);

	/* Get capability to use uart */
	assert(VCAP(uart_cap, 0, VCAP_RW));

	set_uart_cap(uart_cap);
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

	main_loop();

	assert(0);

	while(1);
}
