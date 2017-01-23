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

#define CAP_CAUSE_LIST \
	X(None) \
	X(Length_Violation) \
	X(Tag_Violation) \
	X(Seal_Violation) \
	X(Type_Violation) \
	X(Call_Trap) \
	X(Return_Trap) \
	X(Underflow_of_trusted_system_stack) \
	X(User_defined_Permission_Violation) \
	X(TLB_prohibits_store_capability) \
	X(Requested_bounds_cannot_be_represented_exactly) \
	X(reserved1) \
	X(reserved2) \
	X(reserved3) \
	X(reserved4) \
	X(reserved5) \
	X(Global_Violation) \
	X(Permit_Execute_Violation) \
	X(Permit_Load_Violation) \
	X(Permit_Store_Violation) \
	X(Permit_Load_Capability_Violation) \
	X(Permit_Store_Capability_Violation) \
	X(Permit_Store_Local_Capability_Violation) \
	X(Permit_Seal_Violation) \
	X(Access_System_Registers_Violation) \
	X(reserved6) \
	X(reserved7) \
	X(reserved8) \
	X(reserved9) \
	X(reserved10) \
	X(reserved11) \
	X(reserved12)

#define X(Arg) Arg ,
typedef enum cap_cause_exception_t {
    CAP_CAUSE_LIST
} cap_cause_exception_t;
#undef X

typedef struct cap_exception_t {
    cap_cause_exception_t cause;
    int reg_num;
} cap_exception_t;

static inline cap_exception_t parse_cause(register_t packed_cause) {
	return (cap_exception_t) {.cause = ((packed_cause >> 8) & 0x1F), .reg_num = (packed_cause & 0xFF)};
}

#ifndef __LITE__
	#define X(Arg)  #Arg ,
	static const char * capcausestr[0x20] = {
			CAP_CAUSE_LIST
	};
	#undef X

	#define exception_printf kernel_printf
#else
	#define exception_printf(...)
#endif

static inline const char * getcapcause(int cause) {
#ifndef __LITE__
    return capcausestr[cause];
#else
    return ""; cause++;
#endif
}

#endif //CHERIOS_KERNEL_EXCEPTIONS_H
