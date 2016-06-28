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

typedef struct socket_t {
	int status; /* 0:off / 1:on */
	int oid;
	int socksend;
	void * msg;
} socket_t;

const int MAX_SOCKET = 0x10;
const int MAX_PORT = 0x10;
int ports[MAX_PORT+1];
socket_t sockets[MAX_SOCKET+1];
int next_socket = 0;

void socket_init(void) {
	bzero(sockets, sizeof(sockets));
	for(int i=0; i<=MAX_PORT; i++) {
		ports[i] = -1;
	}
}

int socket(void) {
	int * obj = get_curr_cookie();
	assert(obj != NULL);
	assert(next_socket <= MAX_SOCKET);
	assert(sockets[next_socket].status == 0);
	sockets[next_socket].status = 1;
	sockets[next_socket].oid = *obj;
	sockets[next_socket].socksend = -1;
	sockets[next_socket].msg = NULL;
	int ret = next_socket;
	next_socket++;
	return ret;
}

int bind(int socket, int port) {
	int ret = -1;
	if(socket > MAX_SOCKET || sockets[socket].status == 0) {
		goto end;
	}
	if(ports[port] >= 0) {
		goto end;
	}
	ports[port] = socket;
	ret = 0;

	end:
	return ret;
}

int connect(int socket, int port) {
	int ret = -1;
	if(socket > MAX_SOCKET || sockets[socket].status == 0) {
		goto end;
	}
	if((port > MAX_PORT) || (ports[port] < 0)) {
		goto end;
	}
	int csocket = ports[port];
	if((sockets[csocket].status == 0) || (sockets[csocket].socksend >= 0)) {
		goto end;
	}
	sockets[csocket].socksend = socket;
	sockets[socket].socksend = csocket;
	ret = 0;

	end:
	return ret;
}

void * recfrom(int socket) {
	void * ret = NULL;
	if((socket > MAX_SOCKET) || (sockets[socket].status == 0) ) {
		goto end;
	}
	int * obj = get_curr_cookie();
	assert(obj != NULL);
	if(sockets[socket].oid == *obj) {
		ret = sockets[socket].msg;
		sockets[socket].msg = NULL;
	}

	end:
	return ret;
}

int sendto(int socket, void * msg) {
	int retval = -1;
	if(socket > MAX_SOCKET) {
		retval = -2;
		goto end;
	}
	if(sockets[socket].status == 0) {
		retval = -3;
		goto end;
	}
	int socksend = sockets[socket].socksend;
	if(socksend < 0) {
		retval = -4;
		goto end;
	}
	int * obj = get_curr_cookie();
	assert(obj != NULL);
	if((sockets[socket].oid == *obj) && (sockets[socksend].msg == NULL)) {
		sockets[socksend].msg = msg;
		retval = 0;
		goto end;
	}
	end:
	return retval;
}
