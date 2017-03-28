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

//#define SOCK_DEBUG

#ifdef SOCK_DEBUG
	#define DPRINT(...) printf(__VA_ARGS__)
#else
	#define DPRINT(...)
#endif

#define NB_MSG 0x10

void * s_cb = NULL; /* Sockets activation */
void * s_cs = NULL; /* Sockets cookie */

static char genc(void) {
	static int n = 42424242;
	n = 3*n+685;
	char c = (n >> 10) & 0x7F;
	if(c<0x21 || c>0x7E) {
		return genc();
	}
	return c;
}

static char * genmsg(size_t n) {
	char * p = malloc(n+1);
	if(!p) {
		return p;
	}
	for(size_t i=0; i<n; i++) {
		p[i] = genc();
	}
	p[n] = '\0';
	return p;
}

#define SP 0
void server(void)
{
    printf("S in\n");
    int sock = socket();
    if(sock < 0) {
    	printf("S socket error\n");
    	return;
    }
    DPRINT("S sock:%d\n", sock);
    int port = 4;
    if(bind(sock, port) < 0) {
    	printf("S bind error\n");
    	return;
    }
    DPRINT("S Bind OK on port %d\n", port);
    int nbm = 0;
    for(;;) {
    	void * msg = recfrom(sock);
    	if(!msg) {
    		DPRINT("S recfrom error\n");
    		ssleep(0);
    		continue;
    	}
    	DPRINT("S Got message !! '%s'\n", msg);
    	strtoupper(msg);
    	if(sendto(sock, msg) < 0) {
    		printf("S sendto error\n");
    		continue;
    	}
    	DPRINT("S Sent message !! '%s'\n", msg);
	#if !(SP)
    	if(++nbm==NB_MSG) {
    		break;
    	}
	#endif
    }
}

void client(void)
{
	printf("C in\n");
	int sock = socket();
	if(sock < 0) {
		printf("C socket error\n");
		return;
	}
	DPRINT("C sock:%d\n", sock);

	int port = 5;
	if(bind(sock, port) < 0) {
		printf("C bind error\n");
		return;
	}
	DPRINT("C Bind OK on port %d\n", port);

	connect:
	if(connect(sock, 4) < 0) {
		printf("C connect error\n");
		ssleep(0);
		goto connect;
	}
	DPRINT("C Connect OK\n");

	#if SP
	void * msg = genmsg(0);
	for(size_t i=0; ; i++) {
	#else
	for(size_t i=0; i<NB_MSG; i++) {
		void * msg = genmsg(i);
	#endif
		if(!msg) {
			printf("C genmsg error\n");
			return;
		}
		#if 1
		int retval = sendto(sock, msg);
		if(retval < 0) {
			printf("C sendto error '%d'\n", retval);
			continue;
		}
		#if !(SP)
		printf("C Sent message !! '%s'\n", msg); //race here as msg is overwritten
		#endif
		recfrom :
		ssleep(0);
		void * retmsg = recfrom(sock);
		if(!retmsg) {
			DPRINT("C recfrom error\n");
			goto recfrom;
		}
		#else
		void * retmsg = msg;
		#endif
		#if !(SP)
		printf("C Got  message !! '%s'\n", retmsg);
		free(retmsg);
		#endif
		#if SP
		if(!(i&0xFFF)) {
			printf("C i:%zd\n", i);
		}
		#endif
	}
}

int main(int argc, char *argv[] __unused) {
	printf("User Hello world [%d]\n", argc);

	/* Use the UART module to print an Hello World */
	void * u_ref = namespace_get_ref(1);
	assert(u_ref != NULL);
	void * u_id  = namespace_get_id(1);
	assert(u_id != NULL);
	ccall_c_n(u_ref, u_id, 1, "CCall Hello world\n");

	#if 0
	printf("OK done!\n");
	return 0;
	#endif

	#if 0
	int * null = NULL;
	int a = null[0];
	printf("a:%d\n", a);
	#endif

	#if 1
	mspace mymspace = create_mspace(0,0); // for example
	syscall_puts("mspace ok\n");
	#define mymalloc(bytes)  mspace_malloc(mymspace, bytes)
	#define myfree(mem)  mspace_free(mymspace, mem)
	void * pt1 = mymalloc(1024*1024);
	//CHERI_PRINT_CAP(pt1);
	myfree(pt1);

	for(size_t i=0x60; i<0x80; i++) {
		//printf("L%lx\n", i);
		void * q = mymalloc(i);
		memset(q, 0, i);
		//CHERI_PRINT_CAP(q);
		myfree(q);
	}
	syscall_puts("lin ok\n");

	void * oldq = NULL;
	for(size_t i=0; i<21; i++) {
		//printf("R%lu\n", i);
		size_t len = 1UL << i;
		void * q = mymalloc(len);
		if(q) {
			memset(q, 0, len);
		}
		//CHERI_PRINT_CAP(q);
		myfree(oldq);
		oldq = q;
	}
	syscall_puts("range ok\n");
	for(size_t i=0; i<10; i++) {
		void * q = mymalloc(1UL << 19);
		//CHERI_PRINT_CAP(q);
		myfree(q);
	}
	syscall_puts("memtest ok\n");
	//return 0;
	#endif

	/* Initialize CCalls to the sockets module */
	s_cb = namespace_get_ref(2);
	assert(s_cb != NULL);
	s_cs = get_cookie(s_cb, namespace_get_id(2));
	assert(s_cs != NULL);

	if(argc%2) {
		server();
	} else {
		client();
	}
	printf("I finished [%d]\n", argc);

	return 0;
}
