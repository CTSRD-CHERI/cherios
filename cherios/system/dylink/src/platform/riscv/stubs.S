/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lawrence Esswood
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include "asm.S"
#include "dylink.h"

#define ctlp abi_local
#define cusp abi_unsafe

.macro STUB_BEGIN name
    .p2align 4
    .protected \name
    START_FUNC \name
.endmacro

.macro STUB_END name
    END_FUNC \name
.endmacro

# See the mips stubs.S / dylink.h for the general layout.
# Secure stubs will mostly be performing modifications to the guard_t pointed to by IDC.
# The CTL_t pointed to by IDC goes between callable_taken <-> callable_ready
# The CTL_t pointed to by CRA goes between returnable_ready <-> returnable_used

# Changes *ptr from state "from" to "to". Also adds "bump" to its info. tmp will return the old state.
.macro DOMAIN_CHANGE_UNCHECKED  ptr, from, to, bump, tmp
    li  \tmp, (\to + (\bump << DOMAIN_INFO_SHIFT) - \from)
    camoadd.d.aqrl  \tmp, \tmp, (\ptr)
.endm

.macro DOMAIN_CHANGE_CHECKED_help ptr, from, to, bump, tmp1, tmp2, error_label, constants
1:  clr.d   \tmp1, (\ptr)
.if (\constants)
    andi    \tmp2, \tmp1, DOMAIN_TYPE_MASK
    subi    \tmp2, \tmp2, \from
    bnez    \tmp2, \error_label
    addi    \tmp2, \tmp1, (\to + (\bump << DOMAIN_INFO_SHIFT) - \from)
    csc.d   \tmp2, \tmp2, (\ptr)
.else
    bne     \tmp1, \from, \error_label
    csc.d   \tmp2, \to, (\ptr)
.endif
    bnez    \tmp2, 1b
.endm

# Same as above, but jumps to error_label if state was not "from".
.macro DOMAIN_CHANGE_CHECKED ptr, from, to, bump, tmp1, tmp2, error_label
    DOMAIN_CHANGE_CHECKED_help \ptr, \from, \to, \bump, \tmp1, \tmp2, \error_label, 1
.endm

# Same again but with from and to in registers
.macro DOMAIN_CHANGE_CHECKED_REG ptr, from, to, tmp1, tmp2, error_label
    DOMAIN_CHANGE_CHECKED_help \ptr, \from, \to, 0, \tmp1, \tmp2, \error_label, 0
.endm


.text

###################################################################
# This stub is used the callee side to achieve cross domain calls #
# If we are completely trusting this never gets called            #
###################################################################

STUB_BEGIN  entry_stub
    # TODO: all sorts of secure things. This just forces fallthrough to trusting entry
    # PLT_REG_TARGET (t1) contains an address to jump to
    # Perform CFI check
    DOMAIN_CHANGE_CHECKED ctlp, callable_ready, callable_taken, 0, t0, t2, entry_stub_trap
    # Save cra. The standard entry will reload csp, so we need to save the adjustment.
    clc         csp, CTLP_OFFSET_CSP(ctlp)
    cincoffset  csp, csp, -CAP_SIZE
    csc         cra, 0(csp)
    csc         csp, CTLP_OFFSET_CSP(ctlp)
    # Call function
    cjalr       cra, PLT_REG_TARGET

    # Restore CRA
    clc         cra, 0(csp)
    cincoffset  csp, csp, CAP_SIZE
    csc         csp, CTLP_OFFSET_CSP(ctlp)

    # Exit domain
    DOMAIN_CHANGE_CHECKED ctlp, callable_taken, callable_ready, 0, t0, t2, entry_stub_trap

    # Clear registers TODO: it needs to be the function epilog that clears a0/a1 if need be
    cclear_regs (t0, t1, t2, t3, t4, t5, t6, a2, a3, a4, a5, a6, a7)
    cret
entry_stub_trap:
    trap
STUB_END    entry_stub

###################################################################
# These stubs are used by the caller for cross domain calls       #
###################################################################

# This is the stub used (copied) by the dynamic linker. The more manual stubs are in cheriplt.h.

STUB_BEGIN  plt_stub

j plt_stub

STUB_END    plt_stub

#################################################
# These are the mode stubs, they are not copied #
#################################################

# PLT_REG_TARGET is the target code pointer
# PLT_REG_TARGET_DATA is the target data pointer

# NOTE: Currently, stack passing on RISCV is a bit broken for CheriOS. The standard calling convetion is for the
# caller to place stack passed arguments as a positive offset from SP. However, here we switch stacks. What we
# need to do is to (re)llocate stack arguments on the unsafe stack, and then have them be an explicit extra argument.
# What we will do for now is (unsafely) give the callee a window on the callers stack passed via CSP. This is still not
# enough, as the callee will want to offset from its _own_ stack.
# As a temporary workaround, callees should copy from the callers stack onto their own. This is being done manually
# at present.
# In order to not have to also copy arguments in these trampolines, we will _not_ adjust the SP passed, but will adjust
# The SP we store to ctlp. This might break calls that end up routing to the same domain, so short circuits might be
# needed.

# single domain means that the target has only a single domain for all callers (like the nano kernel)
STUB_BEGIN  plt_common_single_domain
j plt_common_single_domain
STUB_END    plt_common_single_domain

# Trusts _every_ other compartment.
# We have to save csp/cusp to ctlp. put ctlp in s0 to save it.
STUB_BEGIN  plt_common_complete_trusting
    clc         ct5, CTLP_OFFSET_CSP(ctlp)      # ct5 = old saved csp
    cincoffset  ct4, csp, -(3*CAP_SIZE)         # allocate spills (dont actually change SP because of stack passed arguments)
    csc         cs0, 0(ct4)                     # spill s0
    csc         cra, CAP_SIZE(ct4)              # spill cra
    csc         ct4, CTLP_OFFSET_CSP(ctlp)      # save csp for future invocation
    csc         cusp, CTLP_OFFSET_CUSP(ctlp)    # save cusp for future invocation
    csc         ct5, (CAP_SIZE*2)(ct4)

    cllc_rel    cra, plt_common_complete_trusting_return
    cmove       cs0, ctlp
    cinvoke     PLT_REG_TARGET, PLT_REG_TARGET_DATA

plt_common_complete_trusting_return:
    # restore everything (cgp need
    cmove       ctlp, cs0
    clc         csp, CTLP_OFFSET_CSP(ctlp)
    clc         cusp, CTLP_OFFSET_CUSP(ctlp)
    clc         cgp, CTLP_OFFSET_CGP(ctlp)
    clc         ct5, (CAP_SIZE*2)(csp)
    clc         cra, CAP_SIZE(csp)
    clc         cs0, 0(csp)
    cincoffset  csp, csp, (3*CAP_SIZE)
    csc         ct5, CTLP_OFFSET_CSP(ctlp)
    cret
STUB_END    plt_common_complete_trusting

# Trusts only the target compartment
# Same as complete trust, but also sets the state in guard
STUB_BEGIN  plt_common_trusting
    clc         ct5, CTLP_OFFSET_CSP(ctlp)      # ct5 = old saved csp
    cincoffset  ct4, csp, -(3*CAP_SIZE)         # allocate spills (dont actually change SP because of stack passed arguments)
    csc         cs0, 0(ct4)                     # spill s0
    csc         cra, CAP_SIZE(ct4)              # spill cra
    csc         ct4, CTLP_OFFSET_CSP(ctlp)      # save csp for future invocation
    csc         cusp, CTLP_OFFSET_CUSP(ctlp)    # save cusp for future invocation
    csc         ct5, (CAP_SIZE*2)(ct4)

    cllc_rel    cra, plt_common_trusting_return
    cmove       cs0, ctlp
    DOMAIN_CHANGE_UNCHECKED ctlp, callable_taken, callable_ready, 1, t2
    cinvoke     PLT_REG_TARGET, PLT_REG_TARGET_DATA

plt_common_trusting_return:
    # restore everything (cgp need
    cmove       ctlp, cs0
    DOMAIN_CHANGE_CHECKED ctlp, callable_ready, callable_taken, -1, a2, a3, common_trusting_er
    clc         csp, CTLP_OFFSET_CSP(ctlp)
    clc         cusp, CTLP_OFFSET_CUSP(ctlp)
    clc         cgp, CTLP_OFFSET_CGP(ctlp)
    clc         ct5, (CAP_SIZE*2)(csp)
    clc         cra, CAP_SIZE(csp)
    clc         cs0, 0(csp)
    cincoffset  csp, csp, (3*CAP_SIZE)
    csc         ct5, CTLP_OFFSET_CSP(ctlp)
    cret
common_trusting_er:
    trap
STUB_END    plt_common_trusting

# Does not trust the target. Saves/clears all registers, sets up secure return trampoline.
STUB_BEGIN  plt_common_untrusting

    # We allocate on the (TODO: safe) stack a structure that has a pair of a guard_t and ctlp, which we reload

.set STACK_PASSED_ARG_SIZE, (4 * CAP_SIZE)
.set TOTAL_ALLOCATION_SIZE, ((12 + 1 + 1 + 1 + 1) * CAP_SIZE) # 12 saved regs + ra + saved csp + saved ctlp + guard

    cincoffset  ct2, csp, -TOTAL_ALLOCATION_SIZE
    # Swap and save CSP stored in CTLP to the stack (also save cgp/cusp while here)
    clc         ct3, CTLP_OFFSET_CSP(ctlp)
    csc         ct2, CTLP_OFFSET_CSP(ctlp)
    csc         cusp, CTLP_OFFSET_CUSP(ctlp)
    csc         cgp, CTLP_OFFSET_CGP(ctlp)
    csc         ct3, (2 * CAP_SIZE)(ct2)
.macro store reg, index, glob
    csc         \reg, (\index * CAP_SIZE)(ct2)
.endm
    #define SAVE_LIST cra, cs0, cs1, cs2, cs3, cs4, cs5, cs6, cs7, cs8, cs9, cs10, cs11
    # Store saved regs and RA
    foreachi    store, 3,, SAVE_LIST
    # Make and store a guard_t
    cld         t3, 0(ctlp)
    addi        t3, t3, (returnable_ready - callable_taken) + (1 << DOMAIN_INFO_SHIFT)
    csd         t3, 0(ct2)

    # Store CTLP
    csc         ctlp, CAP_SIZE(ct2)

    # FIXME: we really need to bound csp based on an extra argument here that says how many arguments are on the stack.
    # FIXME: temporal safety ensures that its OK to pass this amount of stack to the callee, but we need to know how much.
    # FIXME: for now, I'm just assumng this is some constant, so at least the template of how this looks will be here.

    # Bound CSP to pass stack args
    csetbounds  csp, csp, STACK_PASSED_ARG_SIZE

    # Get a sealing cap
    clc         ct3, CTLP_OFFSET_CDS(ctlp)

    # Sentry that simply invokes to get to untrusting_true_return
    cllc_rel    cra, untrusting_return_trampoline
    csetbounds  cra, cra, 4
    csealentry  cra, cra
    # The actual return we put in saved registers that will be invoked by the trampoline
    cllc_rel    cs0, untrusting_true_return
    cseal       cs0, cs0, ct3
    csetbounds  cs1, ct2, (2 * CAP_SIZE)
    cseal       cs1, cs1, ct3
    # Exit domain (after this, another thread can jump in)
    DOMAIN_CHANGE_UNCHECKED ctlp, callable_taken, callable_ready, 1, t2
    # t0 and t1 are holding PLT_REG_TARGET and PLT_REG_TARGET_DATA. cra s0 and s1 hold sealed thing. kill everything else.
    # FIXME: we still have the problem of callers being required to zero argument registers themselves. they should. but they dont.
    cclear_regs (gp, tp, t2, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, t3, t4, t5, t6)
    cinvoke     PLT_REG_TARGET, PLT_REG_TARGET_DATA
untrusting_return_trampoline:
    # WARN: this is where the non-atomic exception handling bug could occur
    # WARN: it is very important that this stub does not get access to any authority as the caller's IDC will still be present
    cinvoke     cs0, cs1
untrusting_true_return:
    # We need to reload ctlp from the stack allocation.
    # However, we do not trust it yet.
    # We have to check that the guard on the stack is returnable_ready AND has a correct depth
    DOMAIN_CHANGE_CHECKED ctlp, returnable_ready, returnable_used, 0, a2, a3, untrusting_er
    # At the very least ctlp was a return allocation, and was not used, but may not be at the correct level
    # We don't put this in CTLP yet, because we are not sure we want to commit to a return
    clc         ca4, CAP_SIZE(ctlp)
    # We would expect the following if indeed it is at the correct level:
    addi        a2, a2, (callable_ready-returnable_ready)
    # And we will set state to:
    addi        a3, a2, (callable_taken-callable_ready) + (-1 << DOMAIN_INFO_SHIFT)
    DOMAIN_CHANGE_CHECKED_REG ca4, a2, a3, a5, a6, untrusting_er

    # We are now committed to returning so can set ctlp
    cmove       ctlp, ca4

    # Now restore everything
    clc         csp, CTLP_OFFSET_CSP(ctlp)
    clc         cusp, CTLP_OFFSET_CUSP(ctlp)
    clc         cgp, CTLP_OFFSET_CGP(ctlp)
    clc         ct5, (CAP_SIZE*2)(csp)

    .macro restore reg, index, glob
        clc         \reg, (\index * CAP_SIZE)(csp)
    .endm
    # Restore saved regs and ra
    foreachi    restore, 3,, SAVE_LIST
    # Store stack for next return
    csc         ct5, CTLP_OFFSET_CSP(ctlp)
    # Deallocate stack TODO: this will be different when we do the safe stack bits
    cincoffset  csp, csp, TOTAL_ALLOCATION_SIZE
    cret

untrusting_er:
    trap
STUB_END    plt_common_untrusting


