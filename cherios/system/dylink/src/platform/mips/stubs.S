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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
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

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
#include "mips.h"
#include "cheric.h"
#include "dylink.h"
#include "asm.S"
#include "types.h"

#----   is  $cds (domain sealing capability)                        in a virtual reg, caller saved, can get from ctlp
#$c11   is  $csp (stack pointer capability)                         callee saved
#$c10   is  $cusp (unsafe stack pointer capability)                 argument and callee return register

#$idc   is  $ctlp (global pointer capability) / sealed invocation    caller saved, but restored via crd for free
#$c17   is  $cra (return address capability)                        callee saved
#$c18   is  $crd (return data capability)                           callee saved

#----   is  $cnsp (next unsafe stack pointer)                       in a virtual reg, caller saved

# When a new unsafe allocation block is needed, we move up the linked list. "upping the stack"
# when a function exits, we deallocate its blocks (max 2, one static, one dynamic), this is "downing the stack".

# We allocate dynamic safe space in $csp. We allocate dynamic unsafe space $cusp.
# When it comes to freeing unsafe space, we just bump the start of the buffer and return its new value via $cusp
# Unsafe allocations are always at the start of a buffer, maybe followed by safe allocations



# Everything sealed with cds starts with a 64 bit guard. The lower 8 bits are a type. The rest of the bits can be
# used depending on the type. This reduces pressure on sealing types. For example both return closures and domain
# entry function pointers have the same type. But they cannot be interchanged due to this guard.
# The guard should also be used to encode state to protect against multi threading/


# These diagrams are in increasing address order. Stacks grow downwards so the base of a stack is last.

# Layout of capability thread local pointer (CTLP) table:

###########
# Guard   # <-- $ctlp, a guard with type and state
# ex_pcc  #
# ex_idc  #
# ex_c1   #
# csp     # the stack to use on entry
# cusp    # the unsafe stack to use on entry, currently must update this in every prolog as we may be exiting
# cds     # the domain sealer
# cdl     # the domain link procedure
# cgp     # the capability to our globals
# ....    # the rest of our thread local capabilities
###########


# An unsafe stack (not in use):

##############
# cnsp       # Loaded when we up the stack, and stored when we down
############## <--- cusp

# An unsafe stack (in use):

##################
# UNSAFE dynamic # <-- cusp (cnsp stored elsewhere)
##################

# A safe stack:

#################
# SAFE dynamic  # <-- csp
################# <-- cbp
# SAFE static   #
# cpsp          # a pointer back to the previous stack
# cnsp space    # space for cnsp when we empty this stack
# UNSAFE static #
################# <-- cfp


# A return closure. The saves are safe, the guard and ctlp are unsafe:

#########
# Saves #
#########
# Guard # <- idc
# ctlp  # the ctlp to restore
######### # FIXME: There are now ABI requirements for exception handler pcc/idc/c1 here (put a null in ex_pcc / ex_idc?)



.macro ALLOCATE_SAFE_DYNAMIC x, reg
    cincoffset csp, csp, -\x
    csetbounds \reg, csp, \x
.endmacro

.macro ALLOCATE_UNSAFE_DYNAMIC x
    cincoffset $cusp, $cusp, -\x
    csetbounds \reg, $cusp, \x
.endmacro


.macro DOMAIN_CAS_LL  ptr, from, to, bump
    1:  clld    $at, \ptr
.endm

.macro DOMAIN_CAS_SC  ptr, from, to, bump
    andi    $t0, $at, 0xff
    tnei    $t0, \from
    daddiu  $at, $at, (\to + (\bump * 256) - \from)
    cscd    $at, $at, \ptr
    beqz    $at, 1b
.endm

.macro DOMAIN_CAS ptr, from, to, bump
    DOMAIN_CAS_LL \ptr, \from, \to, \bump
    DOMAIN_CAS_SC \ptr, \from, \to, \bump
.endmacro

.macro DOMAIN_CHANGE_LOADS ptr, from, to, bump
    cld     $at, $zero, 0(\ptr)
.endm

.macro DOMAIN_CHANGE_USES ptr, from, to, bump
    daddiu  $at, $at, (\to + (\bump * 256) - \from)
    csd     $at, $zero, 0(\ptr)
.endm

.macro DOMAIN_CHANGE ptr, from, to, bump
    DOMAIN_CHANGE_LOADS \ptr, \from, \to, \bump
    DOMAIN_CHANGE_USES \ptr, \from, \to, \bump
.endmacro

.macro STUB_BEGIN name
    .p2align 3
    .global \name
    .ent \name
    .protected \name
    \name :
.endmacro

.macro STUB_END name
    .p2align 3
    .end \name
.endmacro


.macro BUMP_STAT OFFSET CTMP RTMP
#if DEBUG_COUNT_CALLS
#if (IS_KERNEL)
    clcbi   \CTMP, CTLP_OFFSET_CGP($idc)
    clcbi	\CTMP, %captab20(own_stats)(\CTMP)
#else
    get_tls_sym \CTMP, own_stats
#endif
    clcbi   \CTMP, 0(\CTMP)
    cbtu    \CTMP, 77f
    nop
    cld     \RTMP, $zero, \OFFSET(\CTMP)
    daddiu  \RTMP, \RTMP, 1
    csd     \RTMP, $zero, \OFFSET(\CTMP)
    77:
#endif
.endm

#define trampoline_dest     $c12

#define csp                 $c11
#define cusp                $c10
#define ctlp                $idc
#define cra                 $c17
#define crd                 $c18
#define cgp                 $c25

# really just a tmp

#define cds                 $c14
#define ctmp                $c15
#define ctmp2               $c16

.text

###################################################################
# This stub is used the callee side to achieve cross domain calls #
# If we are completely trusting this never gets called            #
###################################################################

# On entry to this stub we will have a destination and csp will have already been loaded
# We need different masks so might need to duplicate the stub / pass in a mask

STUB_BEGIN entry_stub


# Check our guard
DOMAIN_CAS_LL ctlp, callable_ready, callable_taken, 0
DOMAIN_CAS_SC ctlp, callable_ready, callable_taken, 0

# Save callers cra/crd
clc         csp, $zero, CTLP_OFFSET_CSP(ctlp)
csc         cra, $zero, -(2 * CAP_SIZE)(csp)                    # save caller state for epilog trampoline
csc         crd, $zero, -CAP_SIZE(csp)
BUMP_STAT   STATS_UNTRUSTED_OFFSET, ctmp, $t0

cincoffset  csp, csp, -(2 * CAP_SIZE)

# cgetpcc always in delay slot of block just before standard cross domain entry
# We want to skip BOTH the cgetpcc and loading the stack ptr (which we have changed)
cincoffset  $c12, $c12, 8   #
cmove       crd, ctlp
cjalr       $c12, cra
cincoffset  $c12, $c12, -4  # c12 should point to start of cross domain entry

# Now restore callers cra/crd

clc         cra, $zero, 0(csp)                    # load back callers state

sync
DOMAIN_CHANGE_LOADS ctlp, callable_taken, callable_ready, 0

clc         crd, $zero, CAP_SIZE(csp)             #
cincoffset  csp, csp, (2 * CAP_SIZE)

DOMAIN_CHANGE_USES ctlp, callable_taken, callable_ready, 0

cclearhi    0                                       # TODO all argument regs (not c3) and tmp regs and stacks etc
cclearlo    0                                       # TODO
ccall_slotless cra, crd, 2


STUB_END entry_stub





###################################################################
# These stubs are used by the callee for cross domain calls       #
###################################################################

# This is the stub used by the dynamic linker. The more manual stubs are in cheriplt.h

STUB_BEGIN plt_stub

.p2align CAP_SIZE_BITS

clc         $c1, $zero, (-CAP_SIZE)($c12)       # Load target code
clcbi       $c12, 0(cgp)                        # Load mode (offset filled by linker)
cjr         $c12                                # Jump to mode
clcbi       $c2, 0(ctlp)                        # Load target data in delay slot (offset filled by linker)

STUB_END plt_stub


# If we want dynamic linking but only one security domain we use this common stub
# We treat the nano kernel as in our own domain

STUB_BEGIN plt_common_single_domain
BUMP_STAT       STATS_COMMON_DOMAIN_OFFSET, ctmp, $t0
ccall_slotless $c1, $c2, 2

STUB_END plt_common_single_domain




# If we have more than one domain, but we trust all of them

STUB_BEGIN plt_common_complete_trusting
BUMP_STAT       STATS_COMPLETE_TRUST_OFFSET, ctmp, $t0
cincoffset      csp, csp, -(2*CAP_SIZE)
clcbi           ctmp, CTLP_OFFSET_CSP(ctlp)
csc             cra, $zero, (0)(csp)
csc             csp, $zero, CTLP_OFFSET_CSP(ctlp)
csc             cusp, $zero, CTLP_OFFSET_CUSP(ctlp)
cscbi           ctmp, (CAP_SIZE)(csp)
ccall_l         $c1, $c2, 2, cra
clc             csp, $zero, CTLP_OFFSET_CSP(ctlp)
clc             cusp, $zero, CTLP_OFFSET_CUSP(ctlp)
clc             cgp, $zero, CTLP_OFFSET_CGP(ctlp)
clcbi           ctmp, (CAP_SIZE)(csp)
clc             cra, $zero, (0)(csp)
cincoffset      csp, csp, (CAP_SIZE*2)
cscbi           ctmp, CTLP_OFFSET_CSP(ctlp)
ccall_slotless  cra, crd, 2

STUB_END plt_common_complete_trusting




# If we have more than one domain, we trust this one, but not all

STUB_BEGIN plt_common_trusting
BUMP_STAT       STATS_TRUST_OFFSET, ctmp, $t0
cincoffset      csp, csp, -(2*CAP_SIZE)
clcbi           ctmp, CTLP_OFFSET_CSP(ctlp)
csc             cra, $zero, (0)(csp)
csc             csp, $zero, CTLP_OFFSET_CSP(ctlp)
sync
DOMAIN_CHANGE_LOADS ctlp, callable_taken, callable_ready, 1
csc             cusp, $zero, CTLP_OFFSET_CUSP(ctlp)
cscbi           ctmp, (CAP_SIZE)(csp)
DOMAIN_CHANGE_USES ctlp, callable_taken, callable_ready, 1
ccall_l         $c1, $c2, 2, cra

1:
# Loads are idempotent and so can be used to fill load delay slot
DOMAIN_CAS_LL ctlp, callable_ready, callable_taken, -1
clc             csp, $zero, CTLP_OFFSET_CSP(ctlp)
clc             cusp, $zero, CTLP_OFFSET_CUSP(ctlp)
DOMAIN_CAS_SC ctlp, callable_ready, callable_taken, -1
clc             cgp, $zero, CTLP_OFFSET_CGP(ctlp)
clcbi           ctmp, (CAP_SIZE)(csp)
clc             cra, $zero, (0)(csp)
cincoffset      csp, csp, (CAP_SIZE*2)
cscbi           ctmp, CTLP_OFFSET_CSP(ctlp)
ccall_slotless  cra, crd, 2

STUB_END plt_common_trusting




# If we have more than one domain, and we do not trust the target
# We will need multiple copies of this for different arg numbers in order to clear the correct argument regs
# We give one here that will pass all argument regs (including c13)

# We have 8 cap regs to save (not including stacks). We normally need 2 caps for forward/back, but we are scavenging the one normally used for forward
#define CLO_SAFE_SIZE       ((STACK_LINK_SIZE - CAP_SIZE) + (9 * CAP_SIZE) + (8 * REG_SIZE))
#define CLO_UNSAFE_SIZE     (CAP_SIZE * 2)
#define CLO_SIZE            (CLO_SAFE_SIZE + CLO_UNSAFE_SIZE)

STUB_BEGIN plt_common_untrusting

# FIXME: Now out of date since nanokernel introduced constraints on idc for exceptions

#if (UNSAFE_STACKS_OFF)

#define RETURN_STACK csp
cincoffset      csp,    csp,   -(CLO_SIZE)                         # allocate space for closure without upping stack

#else

#define RETURN_STACK cusp

#WARN: There might be some code that skips this check by knowing its offset. Try to insert code only after the check

# Ensure adequate stack space.
cgetoffset      $1, cusp
tltiu           $1, 0x4000

cscbi           csp, (-CLO_UNSAFE_SIZE + CSP_OFF_PREV)(cusp)
cincoffset      csp,    cusp,   -(CLO_SIZE)                         # allocate space for closure and up the stack
clc             cusp,   $zero,  CSP_OFF_NEXT(cusp)

#endif

BUMP_STAT       STATS_UNTRUSTING_OFFSET, ctmp, $t0

clcbi           ctmp,  CTLP_OFFSET_CSP(ctlp)
csc             csp,    $zero,  CTLP_OFFSET_CSP(ctlp)
csc             cusp,   $zero,  CTLP_OFFSET_CUSP(ctlp)                # store our stacks for future in calls


.set            ctr, 0
.macro SAVE_C reg
    .set ctr, (ctr + CAP_SIZE)
    csc \reg, $zero, (CLO_SAFE_SIZE - ctr)(csp)
.endmacro
.macro SAVE_R reg
    .set ctr, (ctr + REG_SIZE)
    csd \reg, $zero, (CLO_SAFE_SIZE - ctr)(csp)
.endmacro

# Save all our callee saved regs as we do not trust our callee to do it

SAVE_C  $c17  # scavanged forward pointer
.set    ctr, (ctr + CAP_SIZE) # We stored csp here so skip this slot
SAVE_C  $c18
SAVE_C  $c19
SAVE_C  $c20
SAVE_C  $c21
SAVE_C  $c22
SAVE_C  $c23
SAVE_C  $c24
SAVE_C  ctmp
SAVE_R  $s0
SAVE_R  $s1
SAVE_R  $s2
SAVE_R  $s3
SAVE_R  $s4
SAVE_R  $s5
SAVE_R  $s6
SAVE_R  $s7

cmove           $c12, $c1                                                       # required for ABI

.set N_INC,    16
#if (!CAN_SEAL_ANY)
.set N_INC, N_INC + 1
#endif

cgetpcc         cra
cincoffset      cra, cra, (4 * N_INC)

clc             cds,    $zero,  CTLP_OFFSET_CDS(ctlp)                                        # seal our return closure
cincoffset      ctmp,   csp,    CLO_SAFE_SIZE
cseal           cra,    cra,    cds
#if (!CAN_SEAL_ANY)
csetboundsimm   ctmp,   ctmp, CAP_SIZE * 2
#endif
cseal           crd,    ctmp,   cds

cld             $at,    $zero,  0(ctlp)
daddiu          $t0,    $at,    ((returnable_ready - callable_taken) + (1 * 256))
csd             $t0,    $zero,  0(ctmp)                                              # set type of this closure
csc             ctlp,   $zero,  CAP_SIZE(ctmp)
sync

daddiu          $at, $at, ((callable_ready - callable_taken) + (1 * 256))
csd             $at, $zero, 0(ctlp)                                                  # set type of ctlp. Adverary can enter now

cclearhi        0b0000011111111001                                                  # this passes c17/c18
cclearlo        0b1110110000000001                                                  # this passes c1/c2 and arg regs
ccall_slotless  $c1, $c2, 2

# Here is the return code. First check the guard

# idc points to the stack. It is actually already what will be our unsafe stack

2:clld          $at,    $idc
clc             ctmp, $zero, CAP_SIZE($idc)             # we load to ctmp to not clobber idc. We want to do this early so we can fill data load slots
dli             $t1,    returnable_used
andi            $t0,    $at,    0xff
tnei            $t0,    returnable_ready                                            # must first be legal
cscd            $t0,    $t1,    $idc
beqz            $t0,    2b
daddiu          $at,    (callable_ready - returnable_ready)

# We know this is a legal return closure. We can now load ctlp and check it is in a correct state.
#if (CAN_SEAL_ANY)
cmove           RETURN_STACK, $idc                      # actually our current stack, but will be cusp on return so might as well be in this reg
#endif

cmove           ctlp, ctmp

#if (!CAN_SEAL_ANY)
# We need to reload a stack with larger bounds because we were forced to set bounds
clcbi           RETURN_STACK, CTLP_OFFSET_CSP(ctlp)
cincoffset      RETURN_STACK,  RETURN_STACK, CLO_SAFE_SIZE
#endif

# We still might be returning too early
# at contains what we expect to find in our ctlp. Check we have it then mark ourselves as taken

.set            ctr,    0
.macro REST_C reg
    .set ctr, (ctr + CAP_SIZE)
    clc  \reg, $zero, (-ctr)(RETURN_STACK)
.endmacro
.macro REST_R reg
    .set ctr, (ctr + REG_SIZE)
    cld  \reg, $zero, (-ctr)(RETURN_STACK)
.endmacro

2:clld          $t0,    ctlp
REST_C  $c17 # scavanged forward pointer
#if(UNSAFE_STACKS_OFF)
.set    ctr, (ctr + CAP_SIZE) # No back pointer here
#else
REST_C  csp # back pointer
#endif
tne             $t0,    $at
daddiu          $t0,    (callable_taken - callable_ready) + (-1 * 256)
cscd            $t0,    $t0,    $idc
beqz            $t0,    2b

# It is only now safe to access other fields of ctlp, so restore our stacks. We have the stacks,
# Well, we loaded some already but thats OK. We will ignore their values if we trap and we don't leave the compartment with them



# Restore everything saved. If this is an attack this will be junk, but we don't use any of it until the check

REST_C  $c18
REST_C  $c19
REST_C  $c20
REST_C  $c21
REST_C  $c22
REST_C  $c23
REST_C  $c24
REST_C  ctmp2
REST_R  $s0
REST_R  $s1
REST_R  $s2
REST_R  $s3
REST_R  $s4
REST_R  $s5
REST_R  $s6
REST_R  $s7

# We just need to correct the up link on cusp

#if(UNSAFE_STACKS_OFF)
cincoffset      csp, csp, CLO_UNSAFE_SIZE
cscbi           ctmp2, CTLP_OFFSET_CSP(ctlp) # csp should be set to what it was
clc             cgp, $zero, CTLP_OFFSET_CGP(ctlp)
#else

clc             ctmp,   $zero,  CTLP_OFFSET_CUSP(ctlp)
# We modified the csp and cusp values in our ctlp and so have to restore them (although cusp may be different now)
cscbi           ctmp2, CTLP_OFFSET_CSP(ctlp) # csp should be set to what it was
cscbi           cusp, CTLP_OFFSET_CUSP(ctlp) # set to current value of cusp. More up to date values are saved in every frame
clc             cgp, $zero, CTLP_OFFSET_CGP(ctlp)
csc             ctmp,   $zero,  CSP_OFF_NEXT(cusp)

#endif

ccall_slotless  cra, crd, 2

STUB_END plt_common_untrusting
