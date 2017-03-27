/*-
 * Copyright (c) 2013-2015 Robert N. M. Watson
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

#ifndef _EXPORT_CHERIOS_CHERIC_H
#define _EXPORT_CHERIOS_CHERIC_H

int cherios_main(void);

/*
 * Canonical C-language representation of a capability.
 */
typedef __capability void * capability;
typedef __capability const void * const_capability;

/*
 * Register frame to be preserved on context switching. The order of
 * save/restore is very important for both reasons of correctness and security.
 * Assembler routines know about this layout, so great care should be taken.
 */
typedef struct reg_frame {
	/*
	 * General-purpose MIPS registers.
	 */
	/* No need to preserve $zero. */
	register_t	mf_at, mf_v0, mf_v1;
	register_t	mf_a0, mf_a1, mf_a2, mf_a3, mf_a4, mf_a5, mf_a6, mf_a7;
	register_t	mf_t0, mf_t1, mf_t2, mf_t3;
	register_t	mf_s0, mf_s1, mf_s2, mf_s3, mf_s4, mf_s5, mf_s6, mf_s7;
	register_t	mf_t8, mf_t9;
	/* No need to preserve $k0, $k1. */
	register_t	mf_gp, mf_sp, mf_fp, mf_ra;

	/* Multiply/divide result registers. */
	register_t	mf_hi, mf_lo;

	/* Program counter. */
	register_t	mf_pc;

	/*
	 * Capability registers.
	 */
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


} reg_frame_t;

#endif /* _EXPORT_CHERIOS_CHERIC_H */
