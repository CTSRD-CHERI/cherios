/*-
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

#ifndef CHERIOS_NANOKERNEL_H
#define CHERIOS_NANOKERNEL_H

#include "cheric.h"
#include "cheriplt.h"

typedef capability context_t;

#define NANO_KERNEL_IF_LIST(ITEM, ...)                                          \
/* TODO in order to do SGX like things we may have an argument that means "and give them a new capability" */\
/* Creates a context from a intial reg_frame and returns a handle */\
    ITEM(create_context, context_t, (reg_frame_t* initial_state, int pass), __VA_ARGS__)  \
/* Deletes a context, restore_from is ONLY used if a context is destroying itself */\
    ITEM(destroy_context, context_t, (context_t context, context_t restore_from), __VA_ARGS__) \
/* Switch to a handle, and store a handle for the suspended context to the location pointed to by store_to */\
    ITEM(context_switch, void, (context_t restore_from, context_t*  store_to), __VA_ARGS__) \
/* Delays interrupts until exit is called the same number of times OR context_switch is called */\
    ITEM(critical_section_enter, void, (void), __VA_ARGS__) \
    ITEM(critical_section_exit, void, (void), __VA_ARGS__) \
/* TODO a better interface would consist of two things: A chain of contexts to restore, and a chain per exception type.
 * TODO my thoughts are these: In the event of an interrupt, it seems silly to restore a special context that decides
 * TODO who to switch to. If it was a timer interrupt we can just schedule the next person in a list. If it was a
 * TODO specific asyn interrupt, some activation (which might have a high priority) might be waiting on that. In this
 * TODO case we might want to restore that context straight away */ \
    ITEM(set_exception_handler, void, (context_t context), __VA_ARGS__) \
/* FIXME for debug ONLY. When we have proper debugging, this must be removed. It defeats the whole point. */\
    ITEM(unlock_context, reg_frame_t*, (context_t context), __VA_ARGS__)

PLT(nano_kernel_if_t, NANO_KERNEL_IF_LIST)

#define ALLOCATE_PLT_NANO PLT_ALLOCATE(nano_kernel_if_t, NANO_KERNEL_IF_LIST)

#endif //CHERIOS_NANOKERNEL_H
