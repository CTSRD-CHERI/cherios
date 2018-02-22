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
#include "dylink.h"
.include "asm.S"


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

# Layout of globals table:

###########  FIXME: There are now ABI requirements for exception handler pcc/idc/c1 here
# Guard   # <-- $ctlp, a guard with type and state
# csp     # the stack to use on entry
# cusp    # the unsafe stack to use on entry, currently must update this in every prolog as we may be exiting
# cds     # the domain sealer
# cdl     # the domain link procedure
# cgp     # the capability to our globals
###########
# Locals  # the result of our globals
###########


# An unsafe stack (not in use):

############## <--- cusp
# spare slot # Filled in when we up the stack
# cnsp       # Loaded when we up the stack, and stored when we down
##############

# An unsafe stack (in use):

##################
# UNSAFE dynamic # <-- cusp (cnsp stored elsewhere)
##################

# A safe stack:

################# <-- cfp
# UNSAFE static #
# SAFE static   # The first item here is a back pointer to the last csp
################# <-- cbp
# SAFE dynamic  #
################# <-- csp



# A return closure :

#########
# Guard # <- idc (only unsafe thing, padded to one cap)
######### # FIXME: There are now ABI requirements for exception handler pcc/idc/c1 here
# Saves #
#########



.macro ALLOCATE_SAFE_DYNAMIC x, reg
    cincoffset csp, csp, -\x
    csetbounds \reg, csp, \x
.endmacro

.macro ALLOCATE_UNSAFE_DYNAMIC x
    cincoffset $cusp, $cusp, -\x
    csetbounds \reg, $cusp, \x
.endmacro


.macro DOMAIN_CAS ptr, from, to, bump
1:  clld    $at, \ptr
    andi    $t0, $at, 0xff
    tnei    $t0, \from
    daddiu  $at, $at, (\to + (\bump * 256) - \from)
    cscd    $at, $at, \ptr
    beqz    $at, 1b
.endmacro

.macro DOMAIN_CHANGE ptr, from, to, bump
    cld     $at, $zero, 0(\ptr)
    daddiu  $at, $at, (\to + (\bump * 256) - \from)
    csd     $at, $zero, 0(\ptr)
.endmacro

.macro STUB_BEGIN name
    .p2align 3
    .global \name
    .ent \name
    \name :
.endmacro

.macro STUB_END name
    .end \name
.endmacro

#define trampoline_dest     $c12

#define csp                 $c11
#define cusp                $c10
#define ctlp                $idc
#define cra                 $c17
#define crd                 $c18
#define cgp                 $c25

# really just a tmp

#define cds                 $c14
#define ctmp                $c14

.text

###################################################################
# This stub is used the callee side to achieve cross domain calls #
# If we are completely trusting this never gets called            #
###################################################################

# On entry to this stub we will have a destination and csp will have already been loaded
# We need different masks so might need to duplicate the stub / pass in a mask

STUB_BEGIN entry_stub


# Check our guard
DOMAIN_CAS ctlp, callable_ready, callable_taken, 0
nop

# Save callers cra/crd

cincoffset  csp, csp, -(2 * CAP_SIZE)
csc         cra, $zero, 0(csp)                    # save caller state for epilog trampoline
csc         crd, $zero, CAP_SIZE(csp)

# Call intented target

cjalr       $c12, cra
cmove       crd, ctlp

# Now restore callers cra/crd

clc         cra, $zero, 0(csp)                    # load back callers state
clc         crd, $zero, CAP_SIZE(csp)             #
cincoffset  csp, csp, (2 * CAP_SIZE)

sync
DOMAIN_CHANGE ctlp, callable_taken, callable_ready, 0

cclearhi    0                                       # TODO all argument regs (not c3) and tmp regs and stacks etc
ccall       cra, crd, 2
cclearlo    0                                       # TODO


STUB_END entry_stub





###################################################################
# These stubs are used by the callee for cross domain calls       #
###################################################################

# We have one of these stubs per function

STUB_BEGIN plt_stub

.p2align CAP_SIZE_BITS

clc         $c1, $zero, (1*CAP_SIZE)($c12)
clc         $c12, $zero, (2* CAP_SIZE)($c12)   # Load common stub (or unsealed target for single domain)
cjr         $c12                               # jump to mode stub (can we use ccall here to make single domain faster?)
clc         $c2, $zero, 0(ctlp)                 # Load target domain data (0 replaced with %captab(lib_sym)

.p2align CAP_SIZE_BITS
.space (2 * CAP_SIZE) # Each stub is followed by padding to the size of one cap, then c1, then mode

STUB_END plt_stub


# If we want dynamic linking but only one security domain we use this common stub
# We treat the nano kernel as in our own domain

STUB_BEGIN plt_common_single_domain

ccall           $c1, $c2, 2
nop

STUB_END plt_common_single_domain




# If we have more than one domain, but we trust all of them

STUB_BEGIN plt_common_complete_trusting

cincoffset      csp, csp, -CAP_SIZE
csc             cra, $zero, (0)(csp)
csc             csp, $zero, CTLP_OFFSET_CSP(ctlp)
csc             cusp, $zero, CTLP_OFFSET_CUSP(ctlp)
ccall           $c1, $c2, 2
cgetpcc         cra
clc             csp, $zero, CTLP_OFFSET_CSP(ctlp)
clc             cra, $zero, (0)(csp)
clc             cusp, $zero, CTLP_OFFSET_CUSP(ctlp)
clc             cgp, $zero, CTLP_OFFSET_CGP(ctlp)
ccall           cra, crd, 2
cincoffset      csp, csp, CAP_SIZE

STUB_END plt_common_complete_trusting




# If we have more than one domain, we trust this one, but not all

STUB_BEGIN plt_common_trusting

cincoffset      csp, csp, -CAP_SIZE
csc             cra, $zero, (0)(csp)
csc             csp, $zero, CTLP_OFFSET_CSP(ctlp)
csc             cusp, $zero, CTLP_OFFSET_CUSP(ctlp)
sync
DOMAIN_CHANGE ctlp, callable_taken, callable_ready, 1
ccall           $c1, $c2, 2
cgetpcc         cra

1:
DOMAIN_CAS ctlp, callable_ready, callable_taken, -1
clc             csp, $zero, CTLP_OFFSET_CSP(ctlp)
clc             cra, $zero, (0)(csp)
clc             cusp, $zero, CTLP_OFFSET_CUSP(ctlp)
clc             cgp, $zero, CTLP_OFFSET_CGP(ctlp)
ccall           cra, crd, 2
cincoffset      csp, csp, CAP_SIZE

STUB_END plt_common_trusting




# If we have more than one domain, and we do not trust the target
# We will need multiple copies of this for different arg numbers in order to clear the correct argument regs
# We give one here that will pass all argument regs (including c13)

#define CLO_SAFE_SIZE       ( CAP_SIZE + (8 * CAP_SIZE) + (8 * REG_SIZE))
#define CLO_UNSAFE_SIZE     CAP_SIZE
#define CLO_SIZE            (CLO_SAFE_SIZE + CLO_UNSAFE_SIZE)

STUB_BEGIN plt_common_untrusting

# FIXME: Now out of date since nanokernel introduced constraints on idc for exceptions

csc             csp,    $zero,  -(CLO_UNSAFE_SIZE + 2*CAP_SIZE)(cusp)
cincoffset      csp,    cusp,   -(CLO_SIZE)                         # allocate space for closure and up the stack
clc             cusp,   $zero,  CSP_OFF_NEXT(cusp)

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

SAVE_C  $c17
.set    ctr, (ctr + CAP_SIZE) # We stored csp here so skip this slot
SAVE_C  $c18
SAVE_C  $c19
SAVE_C  $c20
SAVE_C  $c21
SAVE_C  $c22
SAVE_C  $c23
SAVE_C  $c24
SAVE_R  $s0
SAVE_R  $s1
SAVE_R  $s2
SAVE_R  $s3
SAVE_R  $s4
SAVE_R  $s5
SAVE_R  $s6
SAVE_R  $s7

cgetpcc         cra
cincoffset      cra, cra, (4 * 14)

clc             cds,    $zero,  CTLP_OFFSET_CDS(ctlp)                                        # seal our return closure
cincoffset      csp,    csp,    (CLO_SIZE - REG_SIZE)
cseal           cra,    cra,    cds
cseal           crd,    csp,    cds

cld             $at,    $zero,  0(ctlp)
daddiu          $t0,    $at,    ((returnable_ready - callable_taken) + (1 * 256))
csd             $t0,    $zero,  0(csp)                                              # set type of this closure
sync

daddiu          $at, $at, ((callable_ready - callable_taken) + (1 * 256))
csd             $at, $zero, 0(ctlp)                                                  # set type of ctlp

cclearhi        0b0000011111111001                                                  # this passes c17/c18
ccall           $c1, $c2, 2
cclearlo        0b1111110000000001                                                  # this passes c1/c2 and arg regs

# Here is the return code

3:
dli             $t1,    returnable_used
2:clld          $at,    $idc
andi            $t0,    $at,    0xff
tnei            $t0,    returnable_ready                                            # must first be legal
cscd            $t0,    $t1,    $idc
beqz            $t0,    2b
daddiu          $at,    (callable_ready - returnable_ready)

# We know this is a legal return closure, but we still might be returning too early
cincoffset      cusp,   $idc,   (REG_SIZE - CLO_UNSAFE_SIZE)                       # this will be our eventual cusp


.set            ctr,    0
.macro REST_C reg
    .set ctr, (ctr + CAP_SIZE)
    clc  \reg, $zero, (-ctr)(cusp)
.endmacro
.macro REST_R reg
    .set ctr, (ctr + CAP_SIZE)
    cld  \reg, $zero, (-ctr)(cusp)
.endmacro

# Restore everything saved

REST_C  $c17
REST_C  csp
REST_C  $c18
REST_C  $c19
REST_C  $c20
REST_C  $c21
REST_C  $c22
REST_C  $c23
REST_C  $c24
REST_R  $s0
REST_R  $s1
REST_R  $s2
REST_R  $s3
REST_R  $s4
REST_R  $s5
REST_R  $s6
REST_R  $s7

# at contains what we expect to find in our ctlp. Check we have it then remark ourselves as taken

2:clld          $t0,    ctlp
tne             $t0,    $at
daddiu          $t0,    (callable_taken - callable_ready) + (-1 * 256)
cscd            $t0,    $t0,    $idc
beqz            $t0,    2b

# It is only now safe to access other fields of ctlp, so restore our stacks. We have the stacks,
# We just need to correct the up link on cusp

clc             ctmp,   $zero,  CTLP_OFFSET_CUSP(ctlp)
# -MIGHT- need to store csp and cusp if someone got the address of this from another function. unsure.
clc             cgp, $zero, CTLP_OFFSET_CGP(ctlp)
ccall           cra,    crd, 2
csc             ctmp,   $zero,  CSP_OFF_NEXT(cusp)

STUB_END plt_common_untrusting