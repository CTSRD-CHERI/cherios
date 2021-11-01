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
int
cp0_status_bd_get(void)
{

	return (cp0_status_get() & MIPS_CP0_CAUSE_BD);
}

int
cp0_status_exl_get(void)
{

	return (cp0_status_get() & MIPS_CP0_STATUS_EXL);
}

void
cp0_status_ie_disable(void)
{
    modify_hardware_reg(NANO_REG_SELECT_STATUS, MIPS_CP0_STATUS_IE, 0);
}

void
cp0_status_ie_enable(void)
{
    modify_hardware_reg(NANO_REG_SELECT_STATUS, MIPS_CP0_STATUS_IE, MIPS_CP0_STATUS_IE);
}

int
cp0_status_ie_get(void)
{

	return (cp0_status_get() & MIPS_CP0_STATUS_IE);
}

/*
 * Routines for managing the CP0  HWREna register, used to
 * determine which hardware registers are accessible via the RDHWR
 * instruction
 */
register_t
cp0_hwrena_get(void)
{
    return modify_hardware_reg(NANO_REG_SELECT_HWRENA, 0, 0);
}

void
cp0_hwrena_set(register_t hwrena)
{

    modify_hardware_reg(NANO_REG_SELECT_HWRENA, ~0, hwrena);
}

/*
 * Routines for managing the CP0 count and compare registers, used to
 * implement cycle counting and timers.
 */
uint32_t
cp0_count_get(void)
{
    return (uint32_t)modify_hardware_reg(NANO_REG_SELECT_COUNT, 0, 0);
}

void
cp0_compare_set(uint32_t compare)
{
    modify_hardware_reg(NANO_REG_SELECT_COMPARE, ~0, compare);
}

/*
 * Routines for managing the CP0 cause register.
 */

register_t
cp0_cause_excode_get(register_t cause)
{

	return ((cause & MIPS_CP0_CAUSE_EXCCODE) >>
	    MIPS_CP0_CAUSE_EXCODE_SHIFT);
}

register_t
cp0_cause_ipending_get(register_t cause)
{

	return ((cause & MIPS_CP0_CAUSE_IP) >>
	    MIPS_CP0_CAUSE_IP_SHIFT);
}