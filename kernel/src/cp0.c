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

#include "mips.h"
#include "cp0.h"

/*
 * CHERI demonstration mini-OS: support routines for MIPS coprocessor 0 (CP0).
 */

/*
 * Routines for managing various aspects of the CP0 status register.
 */
static inline register_t
cp0_status_get(void)
{
	register_t status;

	__asm__ __volatile__ ("dmfc0 %0, $12" : "=r" (status));
	return (status);
}

static inline void
cp0_status_set(register_t status)
{

	__asm__ __volatile__ ("dmtc0 %0, $12" : : "r" (status));
}

int
cp0_status_bd_get(void)
{

	return (cp0_status_get() & MIPS_CP0_CAUSE_BD);
}

void
cp0_status_bev_set(int bev)
{
	register_t status;

	/* XXXRW: Non-atomic test-and-set. */
	status = cp0_status_get();
	if (bev)
		status |= MIPS_CP0_STATUS_BEV;
	else
		status &= ~MIPS_CP0_STATUS_BEV;
	cp0_status_set(status);
}

int
cp0_status_exl_get(void)
{

	return (cp0_status_get() & MIPS_CP0_STATUS_EXL);
}

void
cp0_status_ie_disable(void)
{
	register_t status;

	/* XXXRW: Non-atomic test-and-set. */
	status = cp0_status_get();
	status &= ~MIPS_CP0_STATUS_IE;
	cp0_status_set(status);
}

void
cp0_status_ie_enable(void)
{
	register_t status;

	/* XXXRW: Non-atomic test-and-set. */
	status = cp0_status_get();
	status |= MIPS_CP0_STATUS_IE;
	cp0_status_set(status);
}

int
cp0_status_ie_get(void)
{

	return (cp0_status_get() & MIPS_CP0_STATUS_IE);
}

void
cp0_status_im_enable(int mask)
{
	register_t status;

	/* XXXRW: Non-atomic modification. */
	status = cp0_status_get();
	status |= (mask << MIPS_CP0_STATUS_IM_SHIFT);
	cp0_status_set(status);
}

void
cp0_status_im_disable(int mask)
{
	register_t status;

	/* XXXRW: Non-atomic modification. */
	status = cp0_status_get();
	status &= ~(mask << MIPS_CP0_STATUS_IM_SHIFT);
	cp0_status_set(status);
}

/*
 * Routines for managing the CP0  HWREna register, used to
 * determine which hardware registers are accessible via the RDHWR
 * instruction
 */
register_t
cp0_hwrena_get(void)
{
	register_t hwrena;

	__asm__ __volatile__ ("dmfc0 %0, $7" : "=r" (hwrena));
	return (hwrena & 0xFFFFFFFF);
}

void
cp0_hwrena_set(register_t hwrena)
{

	__asm__ __volatile__ ("dmtc0 %0, $7" : : "r" (hwrena));
}

/*
 * Routines for managing the CP0 count and compare registers, used to
 * implement cycle counting and timers.
 */
register_t
cp0_count_get(void)
{
	register_t count;

	__asm__ __volatile__ ("dmfc0 %0, $9" : "=r" (count));
	return (count & 0xFFFFFFFF);
}

void
cp0_compare_set(register_t compare)
{

	__asm__ __volatile__ ("dmtc0 %0, $11" : : "r" (compare));
}

/*
 * Routines for managing the CP0 cause register.
 */
static register_t
cp0_cause_get(void)
{
	register_t cause;

	__asm__ __volatile__ ("dmfc0 %0, $13" : "=r" (cause));
	return (cause);
}

register_t
cp0_cause_excode_get(void)
{

	return ((cp0_cause_get() & MIPS_CP0_CAUSE_EXCCODE) >>
	    MIPS_CP0_CAUSE_EXCODE_SHIFT);
}

register_t
cp0_cause_ipending_get(void)
{

	return ((cp0_cause_get() & MIPS_CP0_CAUSE_IP) >>
	    MIPS_CP0_CAUSE_IP_SHIFT);
}

/*
 * Routines for managing the CP0 BadVAddr register.
 */
register_t
cp0_badvaddr_get(void)
{
	register_t badvaddr;

	__asm__ __volatile__ ("dmfc0 %0, $8" : "=r" (badvaddr));
	return (badvaddr);
}
