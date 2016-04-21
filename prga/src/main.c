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

void * s_cb = NULL;
void ** s_methods = NULL;

char genc(void) {
	static int n = 42424242;
	n = 3*n+685;
	char c = (n >> 10) & 0x7F;
	if(c<0x21 || c>0x7E) {
		return genc();
	}
	return c;
}

char * genmsg(size_t n) {
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
    	if(++nbm==NB_MSG) {
    		break;
    	}
    }
}

void client(void)
{
    DPRINT("C in\n");
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

    //void * msg = genmsg(0);
    for(size_t i=0; i<NB_MSG; i++) {
    	void * msg = genmsg(i);
    	if(!msg) {
    		printf("C genmsg error\n");
    		return;
    	}
    	int retval = sendto(sock, msg);
    	if(retval < 0) {
    		printf("C sendto error '%d'\n", retval);
    		continue;
    	}
    	printf("C Sent message !! '%s'\n", msg); //race here as msg is overwritten
    	recfrom :{}
    	void * retmsg = recfrom(sock);
    	if(!retmsg) {
    		DPRINT("C recfrom error\n");
    		ssleep(0);
    		goto recfrom;
    	}
    	printf("C Got  message !! '%s'\n", retmsg);
    	free(retmsg);
    	#if 1
    	if(!(i&0xFFF)) {
    		printf("C i:%zd\n", i);
    	}
    	#endif
    }
}

int
main(int argc, __attribute__((unused)) char *argv[])
{
	printf("User Hello world [%d]\n", argc);

	nssleep(3); /* Kernel does not wait for core modules to be loaded yet */

	/* Initialize functions from libuser_init that are CCalls */
	libuser_init(0);

	/* Use the UART module to print an Hello Zolrd */
	void * u_cb = get_object(1);
	void ** u_methods = get_methods(1);
	assert(u_cb != NULL);
	assert(u_methods != NULL);
	void * uartputs = u_methods[1];
	ccall_c_n(uartputs, u_cb, "CCall Hello world\n");

	#if 0
	printf("OK done!\n");
	ssleep(-1);
	#endif

	/* Initialize CCalls to the sockets module */
	s_cb = get_object(2);
	s_methods = get_methods(2);
	assert(s_cb != NULL);
	assert(s_methods != NULL);

	/* argc is the pid; run two instances of this program and they
	   will exchange messages trough the socket module */
	if(argc%2) {
		server();
	} else {
		client();
	}
	printf("I finished [%d]\n", argc);
	
	ssleep(-1);
	return 0;
}
