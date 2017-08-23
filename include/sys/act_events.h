/*-
 * Copyright (c) 2017 Lawrence Esswood
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

#ifndef ACT_EVENTS
#define ACT_EVENTS

#include "types.h"

/* Add more events here! Of the from ITEM(name, once, __VA_ARGS__). Will create an event called name. If once is true
 * the event will happen once then everyone will be unsubscribed. */

#define EVENT_LIST(ITEM, ...)           \
    ITEM(revoke, 1, __VA_ARGS__)        \
    ITEM(terminate, 1, __VA_ARGS__)

#define SUBSCRIBE_OK                    (0)

#define SUBSCRIBE_ALREADY_SUBSCRIBED    (-1)
#define SUBSCRIBE_NOT_SUBSCRIBED        (-2)
#define SUBSCRIBE_INVALID_TARGET        (-3)
#define SUBSCRIBE_INVALID_NOTIFY        (-4)
#define SUBSCRIBE_NO_SERVICE            (-5)

#define EVENT_PORT_NAMES(name, ...) notify_ ## name ## _port, subscribe_ ## name ## _port,  \
unsubscribe_ ## name ## _port, unsubscribe_all ## name ## _port,

typedef enum act_event_port {
    EVENT_LIST(EVENT_PORT_NAMES,)
} act_event_port;


#define EVENT_IF(name, ...) \
int subscribe_ ## name (act_kt target, act_kt notify, capability carg, register_t arg, register_t port);  \
int unsubscribe_## name(act_kt target, act_kt notify, register_t port);                                   \
int unsubscribe_all_ ## name(act_kt target, act_kt notify);                                               \

EVENT_LIST(EVENT_IF,)

void try_set_event_source(void);

extern act_kt event_act;
#endif //ACT_EVENTS
