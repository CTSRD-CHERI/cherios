# -
# Copyright (c) 2020 Lawrence Esswood
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


# My version of memcpy. The other was looking a little dated and didn't function as memmove.
# NOTE: If you want to steal this you only need to define CAP_SIZE (in bytes) and CRETURN (instruction sequence to return)
# NOTE: and maybe fiddle with the align_copy macros if the CAP_SIZE is not 16 bytes

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
#include "mips.h"
#include "asm.S"

#
# __capability void *memcpy(__capability void *dst,
#                            __capability void *src,
#                            size_t len)
# dst: $c3
# src: $c4
# len: $a0
# Copies len bytes from src to dst.  Returns dst.


#define Rdst $c3
#define Rsrc $c4
#define Rlen $a0
#define Roff $a1

#define Tmp0 $t0
#define Tmp1 $t1
#define Tmp2 $t2
#define Tmp3 $t3
#define Tmp4 $at

#define TmpC  $c6
#define TmpC2 $c5

#define RX Tmp3
#define RY Tmp4
#define Ralign Tmp2

.macro align_bit t, x
    cl\()\t     RX, Roff, 0(Rsrc)
    andi        RY, Ralign, \x
    cs\()\t     RX, Roff, 0(Rdst)
    daddu       Roff, Roff, RY
.endm
.macro align_bit_backwards t, x
    cl\()\t     RX, Roff, (-\x)(Rsrc)
    andi        RY, Ralign, \x
    cs\()\t     RX, Roff, (-\x)(Rdst)
    dsubu       Roff, Roff, RY
.endm

# This weirdness copies up to alignment (16 bytes assumed) without any braches
# It works by copying each power of 2 (up to 8), and conditionally moving the offset forward if the amount required has
# a bit set in the corresponding position. Forwards must be used for the start of the buffer, backwards for the end.

.macro align_copy_forward
    align_bit b, 1
    align_bit h, 2
    align_bit w, 4
    align_bit d, 8
.endm

.macro align_copy_backward
    align_bit_backwards b, 1
    align_bit_backwards h, 2
    align_bit_backwards w, 4
    align_bit_backwards d, 8
.endm

		.text

		.global memcpy
		.ent memcpy
		.global memmove
		.type memmove, "function"
		ASM_VISIBILITY memcpy
		ASM_VISIBILITY memmove
memmove:
    # Memcpy should just be memmove (its pretty cheap), this avoids bugs
memcpy:

    beqz        Rlen, end
    # this slot is full

	# Prefer to go forwards if either is possible to keep path warm
	# When dest <= src || dst >= (src +  len) copy forwards.
	# same as if ¬((dest > src) && (dst < (src + len)) copy forwards (preferable logic)
    cgetaddr    Tmp0, Rsrc                          # src
    cgetaddr    Tmp1, Rdst                          # dst
    daddu       Tmp3, Tmp0, Rlen                    # src + len
    sltu        Tmp3, Tmp1, Tmp3                    # dst < (src + len)
    sltu        Tmp4, Tmp0, Tmp1                    # dst > src
    LOG_AND     Tmp3, Tmp3, Tmp4                    # if ¬T3 copy forwards

    andi        Tmp0, Tmp0, (CAP_SIZE-1)            # T0 = alignment of src
    andi        Tmp1, Tmp1, (CAP_SIZE-1)            # T1 = alignment of dst

    sltiu       Tmp2, Rlen, CAP_SIZE

    # branch if T0 != T1 || T2
    LOG_NOT_EQUAL Tmp4, Tmp0, Tmp1
    LOG_OR      Tmp4, Tmp4, Tmp2

    bnez        Tmp4, small_cpy                     # The fast copy assumes it can copy at least a cap
                                                    # and bufers have the same alignment
    li          Roff, 0                             # the offset we have copied (need in all paths so hoisted here)

    bnez        Tmp3, memcpy_backwards
# Forwards version of memcpy T0 = src.align, T1 = dst.align, T2 = small copy
memcpy_forwards:


    # fine for this slot to cover the next integer instruction

unaligned_start:
    dnegu       Ralign, Tmp0
    andi        Ralign, Ralign, (CAP_SIZE-1)        # how many bytes needed to align to a cap
    beqz        Ralign, aligned_start
    # puts a spurios load in the delay slot, but this is fine as we will load this byte in a second anyway

    align_copy_forward

    dsubu       Rlen, Rlen, Ralign                  # adjust length

aligned_start:
    andi        Ralign, Rlen, (CAP_SIZE-1)          # how many byes not aligned to a cap that need copying at the end
    dsubu       Tmp0, Rlen, Ralign                  # Amount to copy in multiple of caps
    beqz        Tmp0, unaligned_end
    # okay to grab next instruction in slot

    daddu       Tmp1, Roff, Tmp0                    # end value that we will break on
    clc         TmpC, Roff, 0(Rsrc)                 # Do an extra load in case we need one for the odd case
    andi        Tmp0, Tmp0, CAP_SIZE                # odd number of caps ?
    bnez        Tmp0, odd_store
    daddu       Roff, Roff, Tmp0                    # adjust base for odd case

# We have 2 data hazard slots so this has been unrolled a bit. 3 copies would be best, but 4 makes the modulo easy.
aligned_copy:
    clc         TmpC2, Roff, 0(Rsrc)
    clc         TmpC, Roff, CAP_SIZE(Rsrc)
    daddiu      Roff, Roff, (2*CAP_SIZE)
    csc         TmpC2, Roff, (CAP_SIZE*(0-2))(Rdst)
odd_store:
    bne         Roff, Tmp1, aligned_copy
    csc         TmpC, Roff, (CAP_SIZE*(1-2))(Rdst)


unaligned_end:
    # We need to do this part BACKWARDS for our idempotent sequence to work
    beqz        Ralign, end
    daddu       Roff, Roff, Ralign                  # points to just past the END

    align_copy_backward
end:

CRETURN

# Can copy less than a CAP, but not zero. Also works as our unaligned copy.
# This loop is multidirectional, it will loop in opposite directions depending on T3
# T0 = src.align, T1 = dst.align, ¬T3 = copy forwards (or raher if T3 is set copy backwards)
small_cpy:
    andi        Tmp0, Rlen, 1                       # odd number of bytes?
    daddiu      Rlen, Rlen, -2                      # make bound inclusive to make break easier (measured in half words)

    daddu       Tmp2, Rlen, Tmp0
    movn        Roff, Tmp2, Tmp3                    # Roff = T3 ? (Len + Odd) : 0

    dnegu       Tmp2, Tmp0
    movn        Rlen, Tmp2, Tmp3                    # Rlen = T3 ? (Odd ? -1 : 0) : Len

    daddu       Tmp2, Tmp2, 2                       # how much to increment by
    dsll        Tmp3, Tmp3, 2
    dsubu       Tmp2, Tmp2, Tmp3                    # T2 = T3 ? -2 : 2

    clb         Tmp1, Roff, 0(Rsrc)                 # extra load to cover odd case

    bnez        Tmp0, odd_small
    dsubu       Roff, Roff, Tmp0                    # adjust offset to cancel if we branch into odd case

# unrolled once to once again cover data delay slot

small_loop:
    clb         Tmp0, Roff, 0(Rsrc)
    clb         Tmp1, Roff, 1(Rsrc)
    csb         Tmp0, Roff, 0(Rdst)
    # Technically there is a stall here, but getting rid of it is
    # too annoying while also making this loop multidirectional
odd_small:
    csb         Tmp1, Roff, 1(Rdst)
    bne         Roff, Rlen, small_loop
    daddu       Roff, Roff, Tmp2

CRETURN

memcpy_backwards:
#FIXME: Just copy the fastpath copy and loop backwards
    trap

memcpy_end:
.size memcpy, memcpy_end - memcpy
.size memmove, memcpy_end - memcpy
.end memcpy
