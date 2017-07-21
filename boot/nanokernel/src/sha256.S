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

.set noreorder
.set nobopt
.set noat

# This is an implementation of sha256 that tries not to spill any registers - due to lack of stack trusted memory being
# In short supply when we don't trust the system.
# It does however 'spill' some GP registers into Capability registers. Don't judge.
# Takes capability arguments for source and destination. Destination will be a copy of source.

# Input #

#define len_arg     $a0
#define IN_CAP      $c3
#define OUT_CAP     $c4

# Capability that holds magic constants '

#define K_CAP       $c5

# Message schedule. This is actually one big shift register of words, each register holds 2 '

#define w0_1        $t0
#define w2_3        $t1
#define w4_5        $t2
#define w6_7        $t3
#define w8_9        $v0
#define w10_11      $v1
#define w12_13      $at
#define w14_15      $t8

# Hash in a weird permutation packed two words per register #

#define h0_4        $t9
#define h1_5        $a1
#define h2_6        $a2
#define h3_7        $a3

# Intermediate values packed two per register in the same permutation as h #

#define ae          $a4
#define bf          $a5
#define cg          $a6
#define dh          $a7

# Some temps #

#define tmp0        $s0
#define tmp1        $s1
#define tmp2        $s2
#define tmp3        $s3

# A temp for the rotate macro #

#define rot_tmp     $s4

# The registers for the un-permutated output. Can be duplicates

#define out_h0_1    $v0
#define out_h2_3    $v1
#define out_h4_5    $t0
#define out_h6_7    $t1

.data

.align 5
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

.text

# *Grumble* stupid ISA doesn't have rotate *grumble* #
.macro Mrotr D, S, V
    srl     rot_tmp, \S, \V
    sll    \D, \S, (32 - \V)
    or      \D, \D, rot_tmp
.endm

.macro TRUNCATE32 R
    dsll    \R, \R, 32
    dsrl    \R, \R, 32
.endm

.global sha256_start
sha256_start:

###############################################
# Create a capability to the k_words in K_CAP #
###############################################

# save the registers we use

csetoffset  $c6, $c6, $s0
csetoffset  $c7, $c7, $s1
csetoffset  $c8, $c8, $s2
csetoffset  $c9, $c9, $s3
csetoffset  $c10, $c10, $s4

dla         tmp0, k_words
dli         tmp1, 64 * 4
csetoffset  K_CAP, $kdc, tmp0
csetbounds  K_CAP, K_CAP, tmp1

##########
# Init H #
##########

dli         h0_4, 0x6a09e667510e527f
dli         h1_5, 0xbb67ae859b05688c
dli         h2_6, 0x3c6ef3721f83d9ab
dli         h3_7, 0xa54ff53a5be0cd19

###########################################################################################################################
# Load first (partial) block. Put length in first two words, then pad with 000...01 to align. Then maybe load a few words #
###########################################################################################################################

.set align_mask, (64-1)


# ***NOTE*** We are assuming here the message is a multiple of TWO WORDS. It makes lots of logic easier.

daddiu      tmp0, len_arg, (16 + align_mask) # We will need at least 4 words extra. 2 for the length field, 2 for the 0000001 piece.
dli         tmp1, ~(align_mask)
and        tmp1, tmp0, tmp1                # Total length of our padded message
dsubu       tmp0, tmp1, len_arg             # Non input bytes. First 2 words are the length. Then zeroes. Then 2 words storing a 1.
move        len_arg, tmp1                   # message length is now with padding

move        w0_1, len_arg
li          w2_3, 0
li          w4_5, 0
li          w6_7, 0
li          w8_9, 0
li          w10_11, 0
li          w12_13, 0
li          w14_15, 0

dla         tmp1, (set1_table_start-16)     # -16: 8 for the length. 8 for a '1'. The rest are zeros (done above)
dadd        tmp1, tmp1, tmp0                # All the shifts for the jump table just work out. Honestly.
jr          tmp1
dneg        tmp1, tmp0                      # hoisted from set1_table_end

set1_table_start: # each of these is 8 bytes long. So shift by 3 to get correct index

b           set1_table_end
li          w2_3, 1
b           set1_table_end
li          w4_5, 1
b           set1_table_end
li          w6_7, 1
b           set1_table_end
li          w8_9, 1
b           set1_table_end
li          w10_11, 1
b           set1_table_end
li          w12_13, 1
b           set1_table_end
li          w14_15, 1

set1_table_end:


cincoffset  IN_CAP, IN_CAP, tmp1                  # Offset by non input bytes so that the cursour is aligned.
cincoffset  OUT_CAP, OUT_CAP, tmp1

dla         tmp1, (chunk_table_begin-16)
dadd        tmp1, tmp1, tmp0
jr          tmp1
nop

chunk_table_begin:
cld         w4_5, $zero, 0x10(IN_CAP)
csd         w4_5, $zero, 0x10(OUT_CAP)
cld         w6_7, $zero, 0x18(IN_CAP)
csd         w6_7, $zero, 0x18(OUT_CAP)
cld         w8_9, $zero, 0x20(IN_CAP)
csd         w8_9, $zero, 0x20(OUT_CAP)
cld         w10_11, $zero, 0x28(IN_CAP)
csd         w10_11, $zero, 0x28(OUT_CAP)
cld         w12_13, $zero, 0x30(IN_CAP)
csd         w12_13, $zero, 0x30(OUT_CAP)
cld         w14_15, $zero, 0x38(IN_CAP)
csd         w14_15, $zero, 0x38(OUT_CAP)

##############
# Hash chunk #
##############

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
    daddu   tmp3, tmp3, \_wi     # acc += w[i]
   Mrotr    tmp0, \_ae, 6       # S1
    and     tmp1, tmp1, \_ae    # ch
   Mrotr    tmp2, \_ae, 11      # S1
    xor     tmp1, tmp1, \_cg    # ch
    xor     tmp0, tmp0, tmp2    # S1
    daddu   tmp3, tmp3, tmp1    # acc += ch
   Mrotr    tmp2, \_ae, 25      # S1
    daddu   tmp3, tmp3, \_dh    # acc += h
    xor     tmp0, tmp0, tmp2    # S1
    dsrl    tmp1, \_ae, 32      # get a to lower to calculate S0
    daddu   tmp3, tmp3, tmp0    # acc += S1
   Mrotr    tmp0, tmp1, 2       # S0
   Mrotr    tmp2, tmp1, 13      # S0
   Mrotr    tmp1, tmp1, 22      # S0
    xor     tmp0, tmp0, tmp2    # S0
    or      tmp2, \_ae, \_bf    # mag
    xor     tmp0, tmp0, tmp1    # S0
    and     tmp1, \_ae, \_bf    # mag
    dsrl    \_dh, \_dh, 32      # get just d
    and     tmp2, tmp2, \_cg    # mag
    daddu   \_dh, \_dh, tmp3    # set h to d + acc
    or      tmp2, tmp2, tmp1    # mag
    daddu   tmp3, tmp3, tmp0    # acc += S0
    dsrl    tmp2, tmp2, 32      # mag >> 32
    daddu   tmp3, tmp3, tmp2    # acc += mag
    TRUNCATE32 \_dh
    dsll    tmp3, tmp3, 32      # move acc to upper bits for packing
    or      \_dh, \_dh, tmp3    # finsihed!
.endm

dsrl    tmp2, w0_1, 32
HASH_ROUND ae, bf, cg, dh, 0, tmp2
HASH_ROUND dh, ae, bf, cg, 4, w0_1
dsrl    tmp2, w2_3, 32
HASH_ROUND cg, dh, ae, bf, 8, tmp2
HASH_ROUND bf, cg, dh, ae, 12 w2_3

daddiu      len_arg, -4
andi        tmp0, len_arg, (64-1)

beqz        tmp0, load_chunk

# Shift W down by 4.

move        tmp3, w2_3
move        w2_3, w6_7
move        w6_7, w10_11
move        w10_11, w14_15
move        w14_15, w0_1
move        w0_1, w4_5
move        w4_5, w8_9
move        w8_9, w12_13

dli         tmp1, (4 * 4)       # used 4 words from K
daddiu      tmp0, tmp0, -16
bltz        tmp0, continue_chunk # skip calculating new w values
cincoffset  K_CAP, K_CAP, tmp1


# calculates w16_17 (plus any i) from previous values
.macro CALC_WINDOW _w16_17, _w0_1, _w2_3, _w8_9, _w10_11, _w14_15
    #w16_17 = w0_1 + w9_10 + s0:s0' + s1:s1'
    # dont use tmp3, its already holding something

    dsrl    tmp1, \_w14_15,32       #s1(w14)
   Mrotr    \_w16_17, \_w0_1, 7          #s0(w1)
   Mrotr    tmp0, tmp1, 17          #s1(w14)
   Mrotr    tmp2, tmp1, 19          #s1(w14)
    srl     tmp1, tmp1, 10          #s1(w14)
    xor     tmp0, tmp0, tmp2        #s1(w14)
   Mrotr    tmp2, \_w0_1, 18            #s0(w1)
    xor     tmp0, tmp0, tmp1        #s1(w14)
    xor     \_w16_17, \_w16_17, tmp2    #s0(w1)
    dsrl    tmp2, \_w10_11, 32              # = w10
    srl     tmp1, \_w0_1, 3             #s0(w1)
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

CALC_WINDOW w12_13, w14_15, tmp3, w4_5, w6_7, w10_11
CALC_WINDOW w14_15, tmp3, w0_1, w6_7,w8_9, w12_13

b           continue_chunk
nop

# update message schedule and repeat a total of 64/4 = 16 times

##########################################################
# Add to hash values. Reset K. Load next chunk or finish #
##########################################################

load_chunk:

.macro VECTOR_ADD A, B, tmp0, tmp1, mask
    and     \tmp0, \A, \mask
    and     \tmp1, \B, \mask
    addu    \A, \A, \B
    daddu   \tmp0, \tmp0, \tmp1
    TRUNCATE32 \A
    or      \A, \A, \tmp0
.endm

dli         tmp2, ~((1 << 32) - 1)
VECTOR_ADD  h0_4, ae, tmp0, tmp1, tmp2
VECTOR_ADD  h1_5, bf, tmp0, tmp1, tmp2
VECTOR_ADD  h2_6, cg, tmp0, tmp1, tmp2
VECTOR_ADD  h3_7, dh, tmp0, tmp1, tmp2

beqz        len_arg, hash_finish
dli         tmp0, 64
csetoffset  K_CAP, K_CAP, $zero
cincoffset  IN_CAP, IN_CAP, tmp0
cincoffset  OUT_CAP, OUT_CAP, tmp0

cld         w0_1, $zero, 0x00(IN_CAP)
csd         w0_1, $zero, 0x00(OUT_CAP)
cld         w2_3, $zero, 0x08(IN_CAP)
csd         w2_3, $zero, 0x08(OUT_CAP)
cld         w4_5, $zero, 0x10(IN_CAP)
csd         w4_5, $zero, 0x10(OUT_CAP)
cld         w6_7, $zero, 0x18(IN_CAP)
csd         w6_7, $zero, 0x18(OUT_CAP)
cld         w8_9, $zero, 0x20(IN_CAP)
csd         w8_9, $zero, 0x20(OUT_CAP)
cld         w10_11, $zero, 0x28(IN_CAP)
csd         w10_11, $zero, 0x28(OUT_CAP)
cld         w12_13, $zero, 0x30(IN_CAP)
csd         w12_13, $zero, 0x30(OUT_CAP)
cld         w14_15, $zero, 0x38(IN_CAP)
b           process_chunk
csd         w14_15, $zero, 0x38(OUT_CAP)

hash_finish:

dli         tmp0, ~((1 << 32) - 1)
dli         tmp3, ((1<<32)-1)

and         out_h0_1, h0_4, tmp0
dsrl        tmp1, h1_5, 32
and         out_h2_3, h2_6, tmp0
or          out_h0_1, out_h0_1, tmp1
dsrl        tmp1, h3_7, 32
dsll        out_h4_5, h0_4, 32
or          out_h2_3, out_h2_3, tmp1
and         tmp1, h1_5, tmp3
dsll        out_h6_7, h2_6, 32
or          out_h4_5, out_h4_5, tmp1
and         tmp1, h3_7, tmp3
or          out_h6_7, out_h6_7, tmp1

# restore

cgetoffset  $s0, $c6
cgetoffset  $s1, $c7
cgetoffset  $s2, $c8
cgetoffset  $s4, $c10

cjr         $c17
cgetoffset  $s3, $c9
# results how in the 4 return registers