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
#ifndef CHERIOS_SHA256_H
#define CHERIOS_SHA256_H

#define SHA256_BLOCK_SIZE   (512 / 8)
#define SHA256_DIGEST_SIZE  (256 / 8)

// These are the registers used to hold the message window. Use these to first the block by value, and set the top bit of length
#define w0_1        $t0
#define w2_3        $t1
#define w4_5        $t2
#define w6_7        $t3
#define w8_9        $v0
#define w10_11      $v1
#define w12_13      $at
#define w14_15      $t8

// These are used to return by value for use by assembly
#define out_h0_1    $v0
#define out_h2_3    $v1
#define out_h4_5    $t0
#define out_h6_7    $t1

#ifndef __ASSEMBLY__
// Both of these will only work for lengths that are multiples of 2 WORDS!
// Also must have source buffer aligned to 64 bits

#ifdef SHA_COPY
// Also returns the hash in a non-ABI complient way by 4 GP registers - used by assembly
// Also optionally accepts the first block by value in a non-ABI complient way by 8 GP registers
    void sha256_copy(size_t length, char* in, char* out);
#else

    typedef struct sha256_hash {
        uint64_t doublewords[4];
    } sha256_hash;

    void sha256(size_t length, uint64_t * in, sha256_hash* hash);
#endif

#endif

#endif //CHERIOS_SHA256_H
