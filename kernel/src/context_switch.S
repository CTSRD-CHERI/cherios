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

    # c3 = from_frame, c4 = to_frame

    # Save state
    cgetdefault $c13
    save_reg_frame $c3, $t0, $c13, $c17

    # At this point we could reschedule 'from', so we are officially in the 'to' activation
    # However, 'to' is currently in an inconsistent state and an exception here would destroy it
    # We need to exit the critical section of 'from'. If an interrupt is then pending we don't
    # Bother restoring 'to' as it would immediately have its context saved. Instead we just jump
    # to the kernel_exception handler.

    dmfc0 $t0, $12
    ori   $t0, 2        # set SR(EXL)
    mtc0  $t0, $12

    # Now we are in an exception we are safe to use all kernel registers

    # Check if we had supressed any interrupts. If we did, we can pretty much treat this like the
    # Beginning of an exception, but skip the part where we save our state (it is already saved)

    # Exit from's critical, we drop ALL levels.
    cgetdefault $kr1c
    dla $t0, critical_state
    csetoffset $kr1c, $kr1c, $t0 # Critical state.

    cld $t1, $zero, CRIT_STATE_CAUSE_OFFSET($kr1c)
    beqz $t1, no_delayed_exception
    csd $zero, $zero, CRIT_STATE_LEVEL_OFFSET($kr1c)   # critical_state.level = 0

    # Here we set the cause register. If we were doing it properly we would modify it to have the faulting
    # Instruction be from the 'to' activation. However, as only the bits to check the type of interrupt are used
    # we will just use this.
    mtc0        $t1, $13
    csd         $zero, $zero, CRIT_STATE_CAUSE_OFFSET($kr1c)

    # Set up exception stack and default capability in the same way as the exception handler
    dla		$t0, __start_exception_stack
    dla		$t1, __size_exception_stack
    csetoffset	$c11, $kdc, $t0
    csetbounds	$c11, $c11, $t1
    move		$sp, $t1
    csetdefault	$kdc

    # We then call kernel_exception as if we had just taken an exception. On return we go back to the exception handler
    cgetpcc     $c12
    dla         $k0,    kernel_exception
    dla         $k1,    kernel_exception_restore
    csetoffset  $c17,   $c12, $k1
    csetoffset  $c12,   $c12, $k0
    cjr         $c12

no_delayed_exception:

    # Restore everything, we don't have a register spare for $c0 so set default while restoring
    # We use exception registers here. These are not used by the critical section check in exception.S

    cmove $kr1c, $c4             # Our frame

    .macro crestore_setc0 greg, offset, frame
        crestore \greg, \offset, \frame
        .if \offset == 0
            csetdefault \greg
        .endif
    .endm

    # Afterwards $kr1c will have pcc, and we will have lost the frame.
    # c0 is not really stored in $c1, we just use it as temporary

    restore_reg_frame_gen crestore_setc0, grestore, $kr1c, $t0, $c1, $epcc

    # finally set pcc, and drop our final level of privilege (the kernel capabilities)
    # Counter eret bug
    cgetoffset $k0, $epcc
    dmtc0      $k0, $14
    eret

nop