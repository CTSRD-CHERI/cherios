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

#define HAS_HIGH_DEF_TIME

extern capability clint_cap;
#define TIMER_COUNT_OFFSET      0xBFF8
#define TIMER_COMPARE_OFFSET    0x4000
#define CLINT_SIP_OFFSET        0

static inline uint8_t cpu_get_cpuid(void) {
    return 0;
}

static inline uint64_t cpu_count_get(void) {
    return *(volatile uint64_t *)((char*)clint_cap + TIMER_COUNT_OFFSET);
}

static inline void cpu_compare_set(uint64_t compare) {
    *(volatile uint64_t *)((char*)clint_cap + TIMER_COMPARE_OFFSET) = compare;
    *(volatile uint32_t *)((char*)clint_cap + CLINT_SIP_OFFSET) = 1;
}

static inline void cpu_enable_timer_interrupts() {
    modify_hardware_reg(NANO_REG_SELECT_sie, RISCV_MIE_STIE, RISCV_MIE_STIE);
}

static inline void cpu_disable_timer_interrupts() {
    modify_hardware_reg(NANO_REG_SELECT_sie, RISCV_MIE_STIE, 0);
}

static inline int cpu_is_timer_interrupt(register_t cause) {
    return (riscv_cause)cause == RISCV_CAUSE_SUPER_TIMER;
}

static inline int cpu_ie_get(void) {
    return (modify_hardware_reg(NANO_REG_SELECT_sstatus, 0, 0)  & RISCV_STATUS_SIE) == RISCV_STATUS_SIE;
}

static inline void cpu_ie_enable() {
    modify_hardware_reg(NANO_REG_SELECT_sstatus, RISCV_STATUS_SIE, RISCV_STATUS_SIE);
}

static inline void cpu_ie_disable() {
    modify_hardware_reg(NANO_REG_SELECT_sstatus, RISCV_STATUS_SIE, 0);
}

#endif //CHERIOS_CPU_H
