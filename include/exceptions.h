/*-
 * Copyright (c) 2018 Lawrence Esswood
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
#ifndef CHERIOS_EXCEPTIONS_H
#define CHERIOS_EXCEPTIONS_H

#include "cheric.h"
#include "dylink.h"

// WARN: If you bump this, make sure to increase the size of the statically sized TLS section in init/init.c
#define USE_EXCEPTION_STACK
#define EXCEPTION_STACK_SIZE 0x1000
#define USE_EXCEPTION_UNSAFE_STACK
#define EXCEPTION_UNSAFE_STACK_SIZE (CAP_SIZE * 0x14) // Used once them replace almost straight away


#ifdef  USE_EXCEPTION_UNSAFE_STACK
    #define USE_EXCEPTION_STACK
#endif

#ifndef __ASSEMBLY__

typedef struct {
    register_t	mf_at, mf_v0, mf_v1;
    register_t	mf_a0, mf_a1, mf_a2, mf_a3, mf_a4, mf_a5, mf_a6, mf_a7;
    register_t	mf_t0, mf_t1, mf_t2, mf_t3;
    register_t	mf_t8, mf_t9;
    register_t	mf_gp, mf_sp, mf_fp, mf_ra;
    register_t	mf_hi, mf_lo;

    register_t  padding;

    capability c2,c3,c4,c5,c6,c7,c8,c9, c12, c13,c14,c15,c16,c17,c18,c25;
#ifdef USE_EXCEPTION_UNSAFE_STACK
    capability c10;
#endif
#ifdef USE_EXCEPTION_STACK
    capability c11;
#endif
} exception_restore_frame;

typedef int handler_t(register_t cause, register_t ccause, exception_restore_frame* restore_frame);

void user_exception_trampoline_vector(void);

//  friendly wrappers for the nano kernel interface. Will do all your stack restoring for you.
//  Return non-zero to replay the exception
//  If you want a fast trampoline that spills fewer registers write a custom one and use raw

// Handle specific excode. Capability exception only handled if cap_vector entry not set.
// Will default to replay
void register_vectored_exception(handler_t* handler, register_t excode);

// Handle specific capability exception. Will default to normal exception if null
void register_vectored_cap_exception(handler_t* handler, register_t excode);

// Raw register. Will override all other usages
void register_exception_raw(ex_pcc_t* exception_pcc, capability exception_idc);

#endif // __ASSEMBLY__

#endif //CHERIOS_EXCEPTIONS_H
