# TODO RISCV

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
#include "sha256.h"


#define IN_CAP      ca0
#define OUT_CAP     ca1
#define len_arg     a2

#define K_CAP       ???

// The message schedule is defined in sha256.h

// Hash values (interleaved for easier unrolling of main loop)
#define h0_4        s0
#define h1_5        s1
#define h2_6        s2
#define h3_7        s3

// Intermediate values

#define ae          s4
#define bf          s5
#define cg          s6
#define dh          s7

// Progress

#define ctr         s8

// temps

#define tmp0        s9
#define tmp1        s10
#define tmp2        s11
#define ctmp0       c ## tmp0



# *Grumble* stupid ISA doesn't have rotate *grumble* #
# 32bit rotate
.macro Mrotr D, S, V
    srlw     rot_tmp, \S, \V
    sllw    \D, \S, (32 - \V)
    or      \D, \D, rot_tmp
.endm
# truncate 64-bit to 32-bits
.macro TRUNCATE32 R
    sll    \R, \R, 32
    srl    \R, \R, 32
.endm

#ifdef SHA_COPY
.text # The nano kernel has only got bss and text!
#else
.data
#endif

.align 5
.global k_words
k_words:
.word	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
.word	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
.word	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
.word	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
.word	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
.word	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
.word	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
.word	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
.word	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
.word	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
.word	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
.word	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
.word	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
.word	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
.word	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
.word	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
.size k_words, (4 * 16 * 4)


.text

# TODO spill (to stack for non-copy, to a pad anotherwise)

#ifdef SHA_COPY
START_FUNC sha256_copy
#else
START_FUNC sha256
#endif

# TODO SPILL

###############################################
# Create a capability to the k_words in K_CAP #
###############################################

// We need to keep getting this because I clobbered K_CAP
#ifdef SHA_COPY
clc         K_CAP, (2 * CAP_SIZE)(abi_local) // Ugly constant
#else
???
#endif

# Init hash to magic values
li          h0_4, 0x6a09e667510e527f
li          h1_5, 0xbb67ae859b05688c
li          h2_6, 0x3c6ef3721f83d9ab
li          h3_7, 0xa54ff53a5be0cd19

li          ctr, 0

srl         tmp1, len_arg, 63
dsll        tmp1, tmp1, 63
xor         len_arg, len_arg, tmp1      # Clear top bit of len_arg either way
bnez        tmp1, process_chunk         # If len_arg has top bit set then the first block is already in the window

b           load_chunk

##########################################################
# Add to hash values. Reset K. Load next chunk or finish #
##########################################################
chunk_end:

# Using a 64-bit registers as a vector of two 32-bit numbers, do A += B
.macro VECTOR_ADD A, B, tmp0, mask
# Remember the xor, but otherwise remove the 31st bit
xor         \tmp0, \A, \B
and         \A, \A, \mask
and         \B, \B, \mask
and         \tmp0, \tmp0, \mask
# Now add
daddu       \A, \A, \B
# And xor in the bit
xor         \A, \A, \tmp0
.endm

li          tmp0, tmp2, ~((1 << 32) - 1)
VECTOR_ADD  h0_4, ae, tmp0, tmp1, tmp2
VECTOR_ADD  h1_5, bf, tmp0, tmp1, tmp2
VECTOR_ADD  h2_6, cg, tmp0, tmp1, tmp2
VECTOR_ADD  h3_7, dh, tmp0, tmp1, tmp2

###################################
# Load a chunk (and maybe pad it) #
###################################

load_chunk:
sub         tmp1, len_arg, ctr
# Because of padding tmp1 will be 16 or more for us to break
li          tmp0, 16
bgeu        tmp1, tmp0, hash_finish
li          tmp0, 0x40
bgeu        tmp1, tmp0, whole_chunk

partial_chunk:

# tmp1 contains how bytes are left. If 0, the 1 goes in w0_1, if 0x8 w2_3, etc
# Conversely, if it is 0 we jump to the _end_ of the whole_chunk table, if its 0x8 the penultimate entry and so forth
get_pcc_label:
auipcc          ctmp0
neg             tmp2, tmp1
#ifndef SHA_COPY
// Each entry in the SHA_COPY table is 8 bytes, but only 4 in the other case
srai            tmp2, tmp1, 1
#endif
addi            tmp2, tmp2, (whole_chunk-get_pcc_label)
cincoffset      ctmp0, ctmp0, tmp2

# Pad with a 1 , 0s, and the length in bytes.
.macro          set_one_or_zero output
    sltiu       \output, tmp1, 1
    slli        \output, \output, 63
    subi        tmp1, tmp1, 1
.endm
set_one_or_zero w0_1
set_one_or_zero w2_3
set_one_or_zero w4_5
set_one_or_zero w6_7
set_one_or_zero w8_9
set_one_or_zero w10_11
set_one_or_zero w12_13
sll             w14_15, len_arg, 3
bltz            # edge case where the padding '1' went in the last chunk
set_one_or_zero w14_15

cjr             ctmp0

whole_chunk:

#ifdef SHA_COPY

cld         w14_15, 0x38(IN_CAP)
csd         w14_15, 0x38(OUT_CAP)
cld         w12_13, 0x30(IN_CAP)   # tmp1 = 0x38
csd         w12_13, 0x30(OUT_CAP)
cld         w10_11, 0x28(IN_CAP)
csd         w10_11, 0x28(OUT_CAP)
cld         w8_9, 0x20(IN_CAP)
csd         w8_9, 0x20(OUT_CAP)
cld         w6_7, 0x18(IN_CAP)
csd         w6_7, 0x18(OUT_CAP)
cld         w4_5, 0x10(IN_CAP)
csd         w4_5, 0x10(OUT_CAP)
cld         w2_3, 0x08(IN_CAP)
csd         w2_3, 0x08(OUT_CAP)
cld         w0_1, 0x00(IN_CAP)     # tmp1 = 8
csd         w0_1, 0x00(OUT_CAP)
                                        # tmp1 = 0
// Even if we only copied a partial block here, we will not use the pointer again if we did
cincoffset  IN_CAP, IN_CAP, SHA256_BLOCK_SIZE
cincoffset  OUT_CAP, OUT_CAP, SHA256_BLOCK_SIZE

#else

cld         w14_15, 0x38(IN_CAP)
cld         w12_13, 0x30(IN_CAP)   # tmp1 = 0x38
cld         w10_11, 0x28(IN_CAP)
cld         w8_9, 0x20(IN_CAP)
cld         w6_7, 0x18(IN_CAP)
cld         w4_5, 0x10(IN_CAP)
cld         w2_3, 0x08(IN_CAP)
cld         w0_1, 0x00(IN_CAP)     # tmp1 = 8
                                        # tmp1 = 0

cincoffset  IN_CAP, IN_CAP, SHA256_BLOCK_SIZE
#endif

############################################
# Perform a hash of the values in w[0..16] #
############################################

process_chunk:

move            ae, h0_4
move            bf, h1_5
move            cg, h2_6
move            dh, h3_7

continue_chunk:
# Permutation is    bd := ac
#                   fh := eg
#                   ac := (T1+T2):b
#                   eg := (d+T2):f     T2 = f(a,b,c)

.macro HASH_ROUND _ae, _bf, _cg, _dh, _koff, _wi
    # ae is the only value that changes. The 'moves' are implict in calling this macro with a permutation
    # puts results in dh and does not change anything else. so d = temp1 + temp2. h = d + temp1.
    clwu    tmp3, $zero, \_koff(K_CAP) # acc = k[i]
    xor     tmp1, \_bf, \_cg    # ch
    addw    tmp3, tmp3, \_wi     # acc += w[i]
   Mrotr    tmp0, \_ae, 6       # S1
    and     tmp1, tmp1, \_ae    # ch
   Mrotr    tmp2, \_ae, 11      # S1
    xor     tmp1, tmp1, \_cg    # ch
    xor     tmp0, tmp0, tmp2    # S1
    addw    tmp3, tmp3, tmp1    # acc += ch
   Mrotr    tmp2, \_ae, 25      # S1
    addw    tmp3, tmp3, \_dh    # acc += h
    xor     tmp0, tmp0, tmp2    # S1
    srl     tmp1, \_ae, 32      # get a to lower to calculate S0
    addw    tmp3, tmp3, tmp0    # acc += S1
   Mrotr    tmp0, tmp1, 2       # S0
   Mrotr    tmp2, tmp1, 13      # S0
   Mrotr    tmp1, tmp1, 22      # S0
    xor     tmp0, tmp0, tmp2    # S0
    or      tmp2, \_ae, \_bf    # mag
    xor     tmp0, tmp0, tmp1    # S0
    and     tmp1, \_ae, \_bf    # mag
    srl     \_dh, \_dh, 32      # get just d
    and     tmp2, tmp2, \_cg    # mag
    addw    \_dh, \_dh, tmp3    # set h to d + acc
    or      tmp2, tmp2, tmp1    # mag
    addw    tmp3, tmp3, tmp0    # acc += S0
    srl     tmp2, tmp2, 32      # mag >> 32
    daddu   tmp3, tmp3, tmp2    # acc += mag
    TRUNCATE32 \_dh
    sll    tmp3, tmp3, 32      # move acc to upper bits for packing
    or      \_dh, \_dh, tmp3    # finsihed!
.endm

dsrl    tmp2, w0_1, 32
HASH_ROUND ae, bf, cg, dh, 0, tmp2
HASH_ROUND dh, ae, bf, cg, 4, w0_1
dsrl    tmp2, w2_3, 32
HASH_ROUND cg, dh, ae, bf, 8, tmp2
HASH_ROUND bf, cg, dh, ae, 12 w2_3

addi            ctr, 4
andi            tmp0, ctr, (0x40-1)
beqz            tmp0, chunk_end

# Shift message window down. tmp3 holds on to w2_3 as we need it to calculate the next part of the window

move        tmp3, w2_3
move        w2_3, w6_7
move        w6_7, w10_11
move        w10_11, w14_15
move        w14_15, w0_1
move        w0_1, w4_5
move        w4_5, w8_9
move        w8_9, w12_13

# Calculate new parts of the window

li              tmp1, 52
bgeu            tmp0, tmp1, continue_chunk # skip calculating new w values
cincoffset      K_CAP, K_CAP, (4*4)        # next few words from K

# calculates w16_17 + i (where i is how many rounds we have done)
.macro CALC_WINDOW _w16_17, _w0_1, _w2_3, _w8_9, _w10_11, _w14_15
    #w16_17 = w0_1 + w9_10 + s0:s0' + s1:s1'
    # dont use tmp3, its already holding something
    # this clobers _w0_1
    srl     tmp1, \_w14_15,32       #s1(w14)
   Mrotr    \_w16_17, \_w0_1, 7          #s0(w1)
   Mrotr    tmp0, tmp1, 17          #s1(w14)
   Mrotr    tmp2, tmp1, 19          #s1(w14)
    srl     tmp1, tmp1, 10          #s1(w14)    # FIXME: 32 bit?
    xor     tmp0, tmp0, tmp2        #s1(w14)
   Mrotr    tmp2, \_w0_1, 18            #s0(w1)
    xor     tmp0, tmp0, tmp1        #s1(w14)
    xor     \_w16_17, \_w16_17, tmp2    #s0(w1)
    srl     tmp2, \_w10_11, 32              # = w10
    srl     tmp1, \_w0_1, 3             #s0(w1) # FIXME: 32 bit
    daddu   tmp2, tmp2, \_w0_1              # = w1 + w10
    dsrl    \_w0_1, \_w0_1, 32              # = w0. w1 no longer needed
    xor     \_w16_17, \_w16_17, tmp1        # = s0
    daddu   \_w0_1, \_w0_1, \_w8_9          # = w0 + w9
    daddu   \_w16_17, \_w16_17, tmp0        # = s0 + s1
    dsrl    tmp1, \_w2_3, 32            #s0'(w2)
    daddu   \_w16_17, \_w16_17, \_w0_1      # = s0 + s1 + w0 + w9
   Mrotr    tmp0, tmp1, 7               #s0'(w2)
   Mrotr    \_w0_1, tmp1, 18            #s0'(w2)
    srl     tmp1, tmp1, 3               #s0'(w2)
    xor     tmp0, tmp0, \_w0_1          #s0'(w2)
   Mrotr    \_w0_1, \_w14_15, 17 #  s1'(w15)
    xor     tmp0, tmp0, tmp1            #s0'(w2)
   Mrotr    tmp1, \_w14_15, 19      #s1'(w15)
    dsll    \_w16_17, \_w16_17, 32          # = (s0 + s1 + w0 + w9) << 32
    xor     \_w0_1, \_w0_1, tmp1    #s1'(w15)
    srl     tmp1, \_w14_15, 10      #s1'(w15)
    daddu   tmp2, tmp2, tmp0                # = w1 + w10 + s0'
    xor     \_w0_1, \_w0_1, tmp1    #s1'(w15)
    daddu   tmp2, tmp2, \_w0_1              # = w1 + s10 + s0' + s1'
    TRUNCATE32 tmp2
    or      \_w16_17, \_w16_17, tmp2        # = (s0 + s1 + w0 + w9) : (w1 + s10 + s0' + s1') as required
.endm
hash_finish:

CALC_WINDOW w12_13, w14_15, tmp3, w4_5, w6_7, w10_11
CALC_WINDOW w14_15, tmp3, w0_1, w6_7,w8_9, w12_13

b           continue_chunk

# Return. We need to unpermute the h values, and either store them, or return by value

# TODO restore any spills
#ifdef SHA_COPY
END_FUNC sha256_copy
#else
END_FUNC sha256
#endif