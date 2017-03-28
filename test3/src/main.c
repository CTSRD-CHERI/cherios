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

register_t cp0_count_get(void)
{
	register_t count;

	__asm__ __volatile__ ("dmfc0 %0, $9" : "=r" (count));
	//__asm__ __volatile__ ("rdhwr %0, $2" : "=r" (count));
	return (count & 0xFFFFFFFF);
}


void * t_ref = NULL;
void * t_id = NULL;
int n = -1;

const int first = 16;
const int last  = first + 32;
const int nmsg = 16;
const int llast = 79;

void lastr(int a, u64 b, int c __unused,
           void * x __unused, void *y __unused, void *z __unused) {
	static u64 sum = 0;
	static u64 cnt = 0;

	__asm("li $0, 0xdd43");
	register_t s = cp0_count_get();
	sum += s-b;
	cnt++;
	//printf("T3: %lu %lu %lu\n", b, s, s-b);
	if(a+1 == nmsg) {
		printf("T3_%dReport: %lu %lu %lu\n", n, sum/cnt, cnt, sum);
	}
	return;
}

extern void msg_entry;
void (*msg_methods[]) = {lastr};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

void fwd(void*, void *);

void ft(void) {
	for(int i=0; i<nmsg; i++) {
		__asm("li $0, 0xdd42");
		ccall_1(t_ref, t_id, 0,
		      i, cp0_count_get(), 0, NULL, NULL, NULL);
		ssleep(0);
		//nssleep(last-first);
	}
}

int main(int argc, char *argv[] __unused)
{
	n = argc;
	//printf("Test3_%d Hello world\n", n);

	/* get ref to Test3A(n+1) */
	if(llast != n) {
		again:{}
		t_ref = namespace_get_ref(n+1);
		if(t_ref == NULL) {ssleep(0); goto again; }
		t_id  = namespace_get_id(n+1);
		assert(t_id != NULL);
	}

	int ret = namespace_register(n, act_self_ref, act_self_id);
	if(ret!=0) {
		printf("Test3_%d: register failed\n", n);
		return -1;
	}

	if(first == n) {
		__asm("li $0, 0xbeef");
		printf("Test3_%d First!\n", n);
		ft();
		printf("Test3_%d Sent last message!\n", n);
	} else if(last == n) {
		msg_enable = 1;
	} else {
		//printf("Test3_%d Tofwd\n", n);
		fwd(t_ref, t_id);
	}


	return 0;
}
