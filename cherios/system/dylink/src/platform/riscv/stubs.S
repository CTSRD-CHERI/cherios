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


# TODO RISCV
.text

###################################################################
# This stub is used the callee side to achieve cross domain calls #
# If we are completely trusting this never gets called            #
###################################################################

STUB_BEGIN  entry_stub
# TODO: all sorts of secure things. This just forces fallthrough to trusting entry
cjr          PLT_REG_TARGET
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
j plt_common_trusting
STUB_END    plt_common_trusting

# Does not trust the target. Saves/clears all registers, sets up secure return trampoline.
STUB_BEGIN  plt_common_untrusting
j plt_common_untrusting
STUB_END    plt_common_untrusting
