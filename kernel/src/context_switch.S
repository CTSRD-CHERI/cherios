# -
# Copyright (c) 2017 Lawrence Esswood
# All rights reserved.
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
# ("CTSRD"), as part of the DARPA CRASH research programme.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

.set MIPS_SZCAP, _MIPS_SZCAP

.include "asm.S"

#define __ASSEMBLY__ 1
#include "syscalls.h"
#include "critical.h"

.text


.global swap_state
swap_state:

    #c3 = from_frame, c4 = to_frame

    # Save state
    cgetdefault $c13
    save_reg_frame $c3, $t0, $c13, $c17

    # At this point we could reschedule 'from', so we are officially in the 'to' activation
    # However, 'to' is currently in an inconsistent state and an exception here would destroy it
    # We need to exit the critical section of 'from', and enter a critical section for the 'to'
    # As we cannot restore AND set a flag, we use two labels to identify this critical region

.global context_critical_start
context_critical_start:

        cgetdefault $c26
        dla $t0, critical_state
        csetoffset $c26, $c26, $t0 # Critical state.
        # Exit from's critical. We assume that we are at level one, or else this is an error.
        csd $zero, $zero, CRIT_STATE_LEVEL_OFFSET($c26)

        # Restore everything, we don't have a register spare for $c0 so set default while restoring
        # We use exception registers here. These are not used by the critical section check in exception.S

        cmove $c26, $c4             # Our frame, and later pcc

        .macro crestore_setc0_keepframe greg, offset, frame
            .ifnc \greg, \frame
                crestore \greg, \offset, \frame
            .endif
            .if \offset == 0
                csetdefault \greg
            .endif
        .endm

        # Restore capability registers, but preserve c26 (the frame ptr)
        # c0 is not really stored in $c1, we just use it as temporary
        restore_reg_frame_gen crestore_setc0_keepframe, grestore, $c26, $t0, $c1, $c26

        # Frame is still in c26, load pcc value instead
        crestore	$c26, 27, $c26

        # finally set pcc, and drop our final level of privilege (the kernel capabilities)
        cjr		$c26
        nop

.global context_critical_end
context_critical_end:

nop
