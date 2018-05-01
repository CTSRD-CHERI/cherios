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

#include "act_events.h"
#include "syscalls.h"
#include "namespace.h"
#include "stdio.h"

act_kt event_act;

void try_set_event_source(void) {
    if(event_act == NULL) {
        event_act = namespace_get_ref(namespace_num_event_service);
    }
}

#define EVENT_IF_BOILERPLATE_subsrcibe(name, ...)                                                                       \
int subscribe_ ## name (act_kt target, act_kt notify, capability carg, register_t arg, register_t port) {               \
    try_set_event_source(); \
    if(event_act == NULL) return SUBSCRIBE_NO_SERVICE;                                                                  \
    return message_send(arg, port, 0, 0, target, notify, carg, NULL, event_act, SYNC_CALL, subscribe_ ## name ## _port); \
}

#define EVENT_IF_BOILERPLATE_unsub(name, ...)                                                                          \
int unsubscribe_ ## name (act_kt target, act_kt notify, register_t port) {                                              \
    if(event_act == NULL) return SUBSCRIBE_NO_SERVICE;                                                                  \
    return message_send(port, 0, 0, 0, target, notify, NULL, NULL, event_act, SYNC_CALL, unsubscribe_ ## name ## _port); \
}

#define EVENT_IF_BOILERPLATE_unsub_all(name, ...)                                                                      \
int unsubscribe_all_ ## name (act_kt target, act_kt notify) {                                                           \
    if(event_act == NULL) return SUBSCRIBE_NO_SERVICE;                                                                  \
    return message_send(0, 0, 0, 0, target, notify, NULL, NULL, event_act, SYNC_CALL, unsubscribe_all ## name ## _port); \
}

#define BOILERPLATE(...)                            \
    EVENT_IF_BOILERPLATE_subsrcibe(__VA_ARGS__)     \
    EVENT_IF_BOILERPLATE_unsub(__VA_ARGS__)         \
    EVENT_IF_BOILERPLATE_unsub_all(__VA_ARGS__)


EVENT_LIST(BOILERPLATE,)