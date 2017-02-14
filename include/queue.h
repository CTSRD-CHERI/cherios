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

#ifndef _CHERIOS_QUEUE_H_
#define	_CHERIOS_QUEUE_H_

#include "cheric.h"
#include "mips.h"

#define MAX_MSG_B 4
#define MAX_MSG (1 << MAX_MSG_B)
typedef size_t msg_nb_t;

/* WARNING
 * The layout of msg_t and queue_t is depended on by libuser/src/msg.S.
 */

typedef struct
{
	capability c3; /* cap arguments */
	capability c4;
	capability c5;

	capability idc; /* identifier */
	capability c1;  /* sync token */
	capability c2;	/* message sender cap */

	/* This serves to align msg_t to a power of 2 * sizeof(capability). It made my life easier. also */
	/* at some point we may wan't more argument passing registers, especially if we use another 2 for a continuation */
#ifdef _CHERI256_
	capability pad;
#endif

	register_t a0; /* GP arguments */
	register_t a1;
	register_t a2;
	register_t v0;  /* method nb */
}  msg_t;

typedef struct
{
	struct header_t {
		volatile msg_nb_t start;
		volatile msg_nb_t end;
		msg_nb_t len;
	} header;
	msg_t msg[0];
}  queue_t;

typedef struct {
	queue_t queue;
	msg_t msgs[MAX_MSG];
} queue_default_t;
#endif