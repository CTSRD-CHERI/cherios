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

#include "exceptions_platform.h"

#ifndef __ASSEMBLY__

void user_exception_trampoline_vector(void);

//  friendly wrappers for the nano kernel interface. Will do all your stack restoring for you.
//  Return non-zero to replay the exception
//  If you want a fast trampoline that spills fewer registers write a custom one and use raw

// Handle specific excode. Capability exception only handled if cap_vector entry not set.
// Will default to replay
void register_vectored_exception(handler_t* handler, register_t excode);

// Same as normal handle but will also provide a saved registers frame
void register_vectored_exception2(handler2_t* handler, register_t excode);

// Handle specific capability exception. Will default to normal exception if null
void register_vectored_cap_exception(handler_t* handler, register_t excode);

// Raw register. Will override all other usages
void register_exception_raw(ex_pcc_t* exception_pcc, capability exception_idc);

#endif // __ASSEMBLY__

#endif //CHERIOS_EXCEPTIONS_H
