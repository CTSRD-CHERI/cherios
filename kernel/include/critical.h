/*-
 * Copyright (c) 2016 Lawrence Esswood
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

#ifndef CHERIOS_CRITICAL_H
#define CHERIOS_CRITICAL_H



#define CRIT_STATE_CAUSE_OFFSET 0
#define CRIT_STATE_LEVEL_OFFSET 8

#ifndef __ASSEMBLY__

#include "cheric.h"
#include "stddef.h"


typedef struct critical_state {
/* If in a critical section the exception handler will set this to getcause and then just return */
    register_t delayed_cause;
/* If not zero we are in a critical section */
    register_t critical_level;
} critical_state_t;

/* For use by the context switcher to save a few registers so it can exit the critical section */
extern critical_state_t critical_state;


void kernel_critical_section_enter();
void kernel_critical_section_exit();
void handle_delayed_interrupt();

/* THese offsets are used by context_switch.S and exception.S */
_Static_assert(offsetof(critical_state_t, delayed_cause) == CRIT_STATE_CAUSE_OFFSET, "offset assumed by assembly");
_Static_assert(offsetof(critical_state_t, critical_level) == CRIT_STATE_LEVEL_OFFSET, "offset assumed by assembly");

#endif

#endif //CHERIOS_CRITICAL_H
