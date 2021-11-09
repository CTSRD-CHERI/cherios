/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lawrence Esswood
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#ifndef CHERIOS_CPU_H
#define CHERIOS_CPU_H

#include "cp0.h"

static inline uint8_t cpu_get_cpuid(void) {
    return cp0_get_cpuid();
}

static inline uint32_t cpu_count_get(void) {
    return cp0_count_get();
}

static inline void cpu_compare_set(uint32_t compare) {
    cp0_compare_set(compare);
}

static inline int cpu_is_timer_interrupt(register_t cause)
{
    return cause & (1 << (MIPS_CP0_INTERRUPT_TIMER + MIPS_CP0_STATUS_IM_SHIFT));
}

static inline int cpu_ie_get(void) {
    return cp0_status_ie_get();
}

static inline void cpu_ie_enable() {
    cp0_status_ie_enable();
}

static inline void cpu_ie_disable() {
    cp0_status_ie_disable();
}

static inline void cpu_enable_timer_interrupts() {
    register_t shifted = (1 << (MIPS_CP0_STATUS_IM_SHIFT+MIPS_CP0_INTERRUPT_TIMER));
    modify_hardware_reg(NANO_REG_SELECT_STATUS, shifted, shifted);
}

static inline void cpu_disable_timer_interrupts() {
    register_t shifted = (1 << (MIPS_CP0_STATUS_IM_SHIFT+MIPS_CP0_INTERRUPT_TIMER));
    modify_hardware_reg(NANO_REG_SELECT_STATUS, shifted, 0);
}

#endif //CHERIOS_CPU_H
