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

.set MIPS_SZCAP, _MIPS_SZCAP

.text
.global cheri_invoke

#TODO save context and seal $11 and $c17 if this is untrusting

# Thoughts #

# We should make this look like a jump and link.

# So with a jump and link we have: pcc = target , and c17 = return
# With a ccall we ALSO have idc = data target, and c18 = data return

# So a normal return is jump c17.
# Now a return is creturn c17 c18

# If the target ALWAYS uses creturn (scrap using jump for function return) it does not matter if the return edge
# is trusting or not. If c17 was unsealed, it is just a jump. if c17 is sealed, we check the c17/c18 pair and do a jump

# By always using idc on function entry to setup our stack pointer and c0, we don't need multiple entries

# The ABI is that parties -SHOULD- obey the normal ABI plus above changes. Unlesss maybe if c17 is sealed, everything
# Is caller saved? It would be weird for a caller to expect a sealed return, but assume its registers were not trampled

# We have 4 cases, with trust/no trust for caller/callee
# If the caller is distrusting it should seal its return context, clear what it wants to
# If the callee is distrusting, it exposes itself to only be accessible by ccall and clears caller saved registers


cheri_invoke:
    # c17/c18 will already be setup with the correct return address / data. May be sealed

    daddiu  $sp,  $sp, 0            # fools my backtracer
    nop
    ccall $c1, $c2, 0