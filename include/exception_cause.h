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

#ifndef CHERIOS_EXCEPTION_CAUSE_H
#define CHERIOS_EXCEPTION_CAUSE_H

#include "string_enums.h"

#define CAP_CAUSE_LIST(ITEM) \
	ITEM(None) \
	ITEM(Length_Violation) \
	ITEM(Tag_Violation) \
	ITEM(Seal_Violation) \
	ITEM(Type_Violation) \
	ITEM(Call_Trap) \
	ITEM(Return_Trap) \
	ITEM(Underflow_of_trusted_system_stack) \
	ITEM(User_defined_Permission_Violation) \
	ITEM(TLB_prohibits_store_capability) \
	ITEM(Requested_bounds_cannot_be_represented_exactly) \
	ITEM(reserved1) \
	ITEM(reserved2) \
	ITEM(reserved3) \
	ITEM(reserved4) \
	ITEM(reserved5) \
	ITEM(Global_Violation) \
	ITEM(Permit_Execute_Violation) \
	ITEM(Permit_Load_Violation) \
	ITEM(Permit_Store_Violation) \
	ITEM(Permit_Load_Capability_Violation) \
	ITEM(Permit_Store_Capability_Violation) \
	ITEM(Permit_Store_Local_Capability_Violation) \
	ITEM(Permit_Seal_Violation) \
	ITEM(Access_System_Registers_Violation) \
	ITEM(Permit_CCall_Violation) \
	ITEM(Access_CCall_IDC_Violation) \
	ITEM(Permit_Unseal_Violation) \
	ITEM(Permit_SetCID_Violation) \
	ITEM(reserved10) \
	ITEM(reserved11) \
	ITEM(reserved12)

DECLARE_ENUM(cap_cause_exception_t, CAP_CAUSE_LIST)

#define CAP_CAUSE_NUM 32
#endif //CHERIOS_EXCEPTION_CAUSE_H
