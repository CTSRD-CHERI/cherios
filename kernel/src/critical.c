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

#include "critical.h"
#include "kutils.h"
#include "cp0.h"

critical_state_t critical_state;

void kernel_critical_section_enter() {
    critical_state.critical_level++;
}


/* Critical exit is only called as a user, and is never in an exception. it CAN be pre-empted. However, the context
 * switch code will also modify critical state. It will first save the caller state, then set SR(EXL) = 1,
 * then set critical level to 0, then check cause. If cause is non-zero it spoofs the exception AS IF IT HAD HAPPENED
 * AT THE ACTIVATION TO BE SCHEDULED. It then enters the normal exception path, which CANNOT be pre-empted.
 *
 * Critical level is a soft interrupt disable. If an interrupt occurs, cause will be set and all interrupts disabled. */

void kernel_critical_section_exit() {
    critical_state.critical_level--;
    if(critical_state.critical_level == 0) {
            handle_delayed_interrupt();
    }
}

void handle_delayed_interrupt() {
    kernel_assert(critical_state.critical_level == 0);
    if(critical_state.delayed_cause != 0) {
        KERNEL_TRACE("critical", "handled a delayed interrupt");
        //FIXME write some code here.
        register_t excode = cp0_cause_excode_get();
        if(excode == MIPS_CP0_EXCODE_INT) {
            kernel_interrupt();
            KERNEL_TRACE("critical", "Now turning interrupts back on");
            // The interrupt delay causes interrupts to be globally disabled. We must now turn them back on
            cp0_status_ie_enable();
        } else {
            kernel_panic("Got non async interrupt in critical section. Fix yo shit.");
        }
        critical_state.delayed_cause = 0;
    }
}