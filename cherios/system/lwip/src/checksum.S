# -
# Copyright (c) 2019 Lawrence Esswood
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
#include "asm.S"
#include "assembly_utils.h"

// We store an array of checksums in our local data. When in not in use they form a free list
#define MAX_SUMS 0x200

#define SEAL_CAP_OFF  (4 * CAP_SIZE)
#define FREE_HEAD_OFF (5 * CAP_SIZE)

#FIXME: Sharing the free list with the data is not secure. Stale references can be used to leak data by allocation
#FIXME: success / failure side channal. To fix this we should tag the union

#FIXME: Also needs to be secure against multi thread attacks

#define FREE_HEAD_START    ((5 * CAP_SIZE) + 2)
#define FREE_HEAD_END      (FREE_HEAD_START + (MAX_SUMS * 2))


#define LOCALS_SIZE ((5*CAP_SIZE) + (MAX_SUMS * 2) + CAP_SIZE)

.data


.p2align CAP_SIZE_BITS

.global checksum_foundation_data
.ent checksum_foundation_data
checksum_foundation_data:


.space LOCALS_SIZE

checksum_found_enter:       # data_arg(table* out_funcs, sealing_cap sealer)
    .global checksum_found_enter_offset
    .set    checksum_found_enter_offset, checksum_found_enter - checksum_foundation_data

# FIXME: really should make this enter once
    li          $t0, LOCALS_SIZE
    csetbounds  $idc, $c8, $t0          # set up locals
    cscbi       $c4, SEAL_CAP_OFF($idc) # store sealing cap for later

    # make funcs
.macro make_func S, X
    daddiu      $t0, $zero, %lo(\S-checksum_found_enter)
    cincoffset  $c6, $c12, $t0
    cseal       $c6, $c6, $c4
    cscbi       $c6, (CAP_SIZE *\X)($c3)
.endm

    make_func   checksum_make_new, 0
    make_func   checksum_free, 1
    make_func   checksum_set, 2
    make_func   checksum_add_int, 3
    make_func   checksum_xor_int, 4
    make_func   checksum_swap_bytes, 5
    make_func   checksum_add_buffer, 6

# init free list

    li          $t0, FREE_HEAD_START - 2 # -2 as we increment before condition / store
    li          $t1, FREE_HEAD_END

1:
    daddiu      $t0, $t0, 2
    bne         $t0, $t1, 1b
    csh         $t0, $t0, -2($idc) # set value of index to index+1. This inits the free list and sets the hd ptr

    cseal       $c3, $idc, $c4          # seal data arg
    csc         $c3, $zero, 0($c5)
    cclearlo    EN5(c8, c9, c12, c4, c3)

    CRETURN

checksum_make_new:          # sealed_sum(uint16_t sum)
    clhu        $a1, $zero, FREE_HEAD_OFF($idc)
    li          $a2, FREE_HEAD_END
    beq         $a1, $a2, no_ret
    clcbi       $c4, SEAL_CAP_OFF($idc)

    clhu            $a2, $a1, 0($idc)
    cincoffset      $c3, $idc, $a1
    csh             $a0, $a1, 0($idc)
    csetboundsimm   $c3, $c3, 2
    csh             $a2, $zero, FREE_HEAD_OFF($idc)
    cseal           $c3, $c3, $c4

    cnull       $c4
    clearlo     EN3(a0, a1, a2)
    CRETURN

checksum_free:
    clcbi       $c4, SEAL_CAP_OFF($idc)
    cunseal     $c3, $c3, $c4
    clhu        $a0, $zero, FREE_HEAD_OFF($idc) # old head
    csub        $a1, $c3, $idc                  # new head
    csh         $a0, $zero, 0($c3)
    b           no_ret
    csh         $a1, $zero, FREE_HEAD_OFF($idc)

checksum_set:               # void(sealed_sum sum, uint16_t val)
    clcbi       $c4, SEAL_CAP_OFF($idc)
    cunseal     $c3, $c3, $c4
    b no_ret
    csh         $a0, $zero, 0($c3)

checksum_add_int:           # void(sealed_sum acc, uint16_t val)
    clcbi       $c4, SEAL_CAP_OFF($idc)
    cunseal     $c3, $c3, $c4
    clhu        $a1, $zero, 0($c3)
    andi        $a0, $a0, 0xFFFF
    daddu       $a0, $a0, $a1
    dsrl        $a1, $a0, 16
    daddu       $a0, $a0, $a1
    andi        $a0, $a0, 0xFFFF
    b no_ret
    csh         $a0, $zero, 0($c3)

checksum_xor_int:            # void(sealed_sum acc, uint16_t val)
    clcbi       $c4, SEAL_CAP_OFF($idc)
    cunseal     $c3, $c3, $c4
    clhu        $a1, $zero, 0($c3)
    xor         $a0, $a0, $a1
    b no_ret
    csh         $a0, $zero, 0($c3)

checksum_swap_bytes:       # void(sealed_sum acc)
    clcbi       $c4, SEAL_CAP_OFF($idc)
    cunseal     $c3, $c3, $c4
    clhu        $a0, $zero, 0($c3)
    dsrl        $a1, $a0, 8
    dsll        $a0, $a0, 8
    or          $a0, $a0, $a1
    b no_ret
    csh         $a0, $zero, 0($c3)

checksum_add_buffer:        # void(sealed_sum acc, capability sealed_data, size_t length)
    beqz        $a0, no_ret
    clcbi       $c5, SEAL_CAP_OFF($idc)
    cgetsealed  $a2, $c4
    beqz        $a2, no_unseal
    cunseal     $c3, $c3, $c5
    cunseal     $c4, $c4, $c5
no_unseal:
    cincoffset  $c4, $c4, $a1
    cgetaddr    $a3, $c4
    clhu        $a1, $zero, 0($c3)              # a1 is accu

    andi        $a2, $a3, 1
    beqz        $a2, 1f
    dnegu       $a3, $a3

# In the odd case swap bytes

.macro SWAP_BYTES X, T
    dsrl        \T, \X, 8
    andi        \X, \X, 0xFF
    dsll        \X, \X, 8
    or          \X, \X, \T
.endm

    cmove       $c5, $cnull                     # we are out of ints. Use c5 = null to remember is odd
    SWAP_BYTES  $a1, $a2

# first copy 1,2,4 to align src

1:  # Copy 1 byte
    andi        $a2, $a3, 1
    beqz        $a2, 1f
    clbu        $a2, $zero, 0($c4) # This goes in the little end. The swap will put in the big end
    cincoffset  $c4, $c4, 1
    daddu       $a1, $a1, $a2
    daddiu      $a0, $a0, -1

1: # Copy 2 byes
    daddiu      $a2, $a0, -2
    bltz        $a2, end_bytes
    andi        $a2, $a3, 2
    beqz        $a2, 1f
    clhu        $a2, $zero, 0($c4)
    cincoffset  $c4, $c4, 2
    daddu       $a1, $a1, $a2
    daddiu      $a0, $a0, -2

1: # Copy 4 bytes
    daddiu      $a2, $a0, -4
    bltz        $a2, end_bytes
    andi        $a2, $a3, 4
    beqz        $a2, 1f
    clwu        $a2, $zero, 0($c4)
    cincoffset  $c4, $c4, 4
    daddu       $a1, $a1, $a2
    daddiu      $a0, $a0, -4
1: # Copy multiple 8 bytes

# Loop footer without the hoisted instruction
    daddiu      $a0, $a0, -8
    bltz        $a0, loop_exit
    nop

# TODO might consider unrolling this once more and do 16 bytes at a time
loop_body:
    cld         $a2, $zero, 0($c4)
    cincoffset  $c4, $c4, 8
    daddu       $a2, $a1, $a2                   # add loaded value to ctr
    sltu        $a3, $a2, $a1                   # carry bit
loop_footer:
    daddiu      $a0, $a0, -8
    bgez        $a0, loop_body
    daddu       $a1, $a2, $a3                   # add carry bit to counter (hoisted from body)

loop_exit:
    daddiu      $a0, $a0, 8                     # we took a bit much off the length

end_bytes:

    # Then do 4 bytes, 2bytes, 1byte. Also fold the value down to accumalator to 2 bytes

# Begin fold 8 to 4
    dsrl        $a2, $a1, 32
    dsll        $a1, $a1, 32
    dsrl        $a1, $a1, 32

# Copy 4 bytes
    andi        $a3, $a0, 4
    beqz        $a3, 1f
    daddu       $a1, $a1, $a2                   # Fold 8 to 4 (and maybe an extra bit in byte 5)

    clwu        $a2, $zero, 0($c4)
    cincoffset  $c4, $c4, 4
    daddu       $a1, $a1, $a2                   # can't overflow due to earlier fold

1:
    # At this point we have mostly finished fold 8 to 4, there might be a little bit in byte 5

# begin fold from 4 to 2
    dsrl        $a2, $a1, 16 # also has a bit in byte 3
    andi        $a1, $a1, 0xFFFF
# Copy 2 bytes
    andi        $a3, $a0, 2
    beqz        $a3, 1f
    daddu       $a1, $a1, $a2                   # fold 4 to 2 (and maybe more extra bits in byte 3)
    clhu        $a2, $zero, 0($c4)
    cincoffset  $c4, $c4, 2
    daddu       $a1, $a1, $a2

# Copy 1 bytes

1:
    andi        $a3, $a0, 1
    beqz        $a3, 1f
    nop
    clbu        $a2, $zero, 0($c4)
    dsll        $a2, $a2, 8                     # Goes in big end, will be swapped to little
    daddu       $a1, $a1, $a2

1:

# Finish folding down to 2 (might need to happen twice?)
    dsrl        $a2, $a1, 16
    andi        $a1, $a1, 0xFFFF
    daddu       $a1, $a1, $a2

    dsrl        $a2, $a1, 16
    andi        $a1, $a1, 0xFFFF

    cbnz        $c5, finish
    daddu       $a1, $a1, $a2                   # finish fold 4 to 2

    SWAP_BYTES  $a1, $a2

finish:
    csh         $a1, $zero, 0($c3)

no_ret:
    cclearlo EN6(c1,c2,c3,c4,c5,c6)
    clearlo  EN4(a0,a1,a2,a3)
    CRETURN

.p2align CAP_SIZE_BITS
.global checksum_foundation_data_end
checksum_foundation_data_end:
.end checksum_foundation_data
#.size    checksum_foundation_data, checksum_foundation_data - checksum_foundation_data_end
