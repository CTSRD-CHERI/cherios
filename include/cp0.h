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

#ifndef _CHERI_CP0_H_
#define	_CHERI_CP0_H_

#include "nano/nanokernel.h"

/*
 * CP0 manipulation routines.
 */
int	cp0_status_bd_get(void);
int	cp0_status_exl_get(void);
void	cp0_status_ie_disable(void);
void	cp0_status_ie_enable(void);
int	cp0_status_ie_get(void);
register_t	cp0_cause_excode_get(register_t cause);
register_t	cp0_cause_ipending_get(register_t cause);
uint32_t	cp0_count_get(void);
void	cp0_compare_set(uint32_t compare);
register_t cp0_hwrena_get(void);
void cp0_hwrena_set(register_t hwrena);

static inline uint8_t cp0_get_cpuid(void) {

	register_t EBase = modify_hardware_reg(NANO_REG_SELECT_EBASE, 0, 0);
	return (char)EBase;
}

static inline register_t
cp0_status_get(void)
{
	register_t status = modify_hardware_reg(NANO_REG_SELECT_STATUS, 0, 0);
	return (status);
}

static inline void
cp0_status_set(register_t status)
{
    modify_hardware_reg(NANO_REG_SELECT_STATUS, ~0, status);
}

#endif /* _CHERI_CP0_H_ */
