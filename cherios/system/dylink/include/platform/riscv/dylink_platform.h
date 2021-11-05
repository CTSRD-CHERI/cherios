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

#ifndef CHERIOS_DYLINK_PLATFORM_H
#define CHERIOS_DYLINK_PLATFORM_H

#define TEMPORAL_TRAP_CODE 0

#define PLT_REG_GLOB            c3
#define PLT_REG_LOCAL           c31
#define PLT_REG_LINK            c1
#define PLT_REG_STACK           c2
#define PLT_REG_UNSAFE_STACK    c4

// For calling plt stubs
#define PLT_REG_TARGET          c1
#define PLT_REG_TARGET_DATA     ct0
#define PLT_REG_RETURN_DATA     ct0

// TODO RISCV
#define get_sym_captable_offset32(Sym) 0
#define get_sym_call_captable_offset32(Sym) 0
#define get_tls_sym_captable_ndx16(Sym) 0

#define get_cgp() cheri_getreg(X_STRINGIFY(PLT_REG_GLOB))
#define get_unsafe_stack_reg() cheri_getreg(X_STRINGIFY(PLT_REG_UNSAFE_STACK))
#define get_safe_stack_reg() cheri_getreg(X_STRINGIFY(PLT_REG_STACK))
#define get_return_reg()  cheri_getreg(X_STRINGIFY(PLT_REG_LINK))
#define get_function_start_reg() cheri_getreg(X_STRINGIFY(PLT_REG_TARGET))
#define get_return_data_reg() cheri_getreg(X_STRINGIFY(PLT_REG_GLOB))

#define set_unsafe_stack_reg(X) cheri_setreg(X_STRINGIFY(PLT_REG_UNSAFE_STACK), X)

#endif //CHERIOS_DYLINK_PLATFORM_H
