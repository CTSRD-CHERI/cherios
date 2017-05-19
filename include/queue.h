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

#define MAX_MSG_B 4
#define MAX_MSG (1 << MAX_MSG_B)

#define MSG_NB_T_SIZE 8
#define HEADER_START_OFFSET 0
#define HEADER_END_OFFSET MSG_NB_T_SIZE
#define HEADER_LEN_OFFSET (MSG_NB_T_SIZE * 2)
#define MSGS_START_OFFSET 32

#ifndef __ASSEMBLY__

#include "cheric.h"
#include "mips.h"
#include "stddef.h"

typedef size_t msg_nb_t;

/* WARNING
 * The layout of msg_t and queue_t is depended on by libuser/src/msg.S.
 */

typedef struct
{
	capability c3; /* cap arguments */
	capability c4;
	capability c5;

	/* Deprecated. Nobody should use this */
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

struct header_t {
	volatile msg_nb_t start;
	volatile msg_nb_t end;
	msg_nb_t len;
};

typedef struct
{
	struct header_t header;
	msg_t msg[0];
}  queue_t;

typedef struct {
	queue_t queue;
	msg_t msgs[MAX_MSG];
} queue_default_t;

_Static_assert(sizeof(msg_nb_t) == MSG_NB_T_SIZE, "size used by msg.S");
_Static_assert((offsetof(queue_t, header) + offsetof(struct header_t, start)) == HEADER_START_OFFSET, "offset used by msg.S");
_Static_assert((offsetof(queue_t, header) + offsetof(struct header_t, end)) == HEADER_END_OFFSET, "offset used by msg.S");
_Static_assert((offsetof(queue_t, header) + offsetof(struct header_t, len)) == HEADER_LEN_OFFSET, "offset used by msg.S");
_Static_assert((offsetof(queue_t, msg) == MSGS_START_OFFSET), "offset used by msg.S");

#endif // __ASSEMBLY__

#endif
