/*-
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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

#ifndef CHERIOS_KERNEL_EXCEPTIONS_H
#define CHERIOS_KERNEL_EXCEPTIONS_H

#include "exception_cause.h"

#ifndef __ASSEMBLY__

#include "activations.h"

typedef struct cap_exception_t {
    cap_cause_exception_t cause;
    int reg_num;
} cap_exception_t;

#ifdef PLATFORM_mips

static inline cap_exception_t parse_cause(register_t packed_cause) {
	return (cap_exception_t) {.cause = ((packed_cause >> 8) & 0x1F), .reg_num = (packed_cause & 0xFF)};
}

#else

static inline cap_exception_t parse_cause(register_t packed_cause) {
    return (cap_exception_t) {.cause = (packed_cause & 0x1F), .reg_num = (packed_cause >> 5) & 0x1F};
}

#endif


void kernel_exception_tlb(register_t badvaddr, act_t* kernel_curr_act);

#ifndef __LITE__
	#define exception_printf kernel_printf
#else
	#define exception_printf(...)
#endif

#endif // __ASSEMBLY__

#endif //CHERIOS_KERNEL_EXCEPTIONS_H
