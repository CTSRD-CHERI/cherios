/*-
 * Copyright (c) 2011 Robert N. M. Watson
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

#ifndef _CHERIDEMO_CP2_H_
#define	_CHERIDEMO_CP2_H_

#include "mips.h"

/*
 * Canonical C-language representation of a capability.
 */
typedef void * capability;

/*
 * Register frame to be preserved on context switching -- very similar to
 * struct mips_frame.  As with mips_frame, the order of save/restore is very
 * important for both reasons of correctness and security.
 */
struct cp2_frame {
	/* c0 has special properties for MIPS load/store instructions. */
	capability	cf_c0;

	/*
	 * General purpose capability registers.
	 */
	capability	cf_c1, cf_c2, cf_c3, cf_c4;
	capability	cf_c5, cf_c6, cf_c7;
	capability	cf_c8, cf_c9, cf_c10, cf_c11, cf_c12;
	capability	cf_c13, cf_c14, cf_c15, cf_c16, cf_c17;
	capability	cf_c18, cf_c19, cf_c20, cf_c21, cf_c22;
	capability	cf_c23, cf_c24, cf_c25;

	/*
	 * Special-purpose capability registers that must be preserved on a
	 * user context switch.  Note that kernel registers are omitted.
	 */
	capability	cf_idc;

	/* Program counter capability. */
	capability	cf_pcc;
};

#endif /* _CHERIDEMO_CP2_H_ */
