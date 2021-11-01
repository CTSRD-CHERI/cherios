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

#define get_cgp() ((capability*)({                           \
capability* __ret;                                             \
__asm__ ("cmove %[ret], $c25" : [ret]"=C"(__ret) ::);     \
__ret;}))

#define CLCBI_IM_OFFSET 2
#define CLCBI_IM_SCALE  4

#define get_sym_captable_offset32(Sym) (uint32_t)({                     \
uint32_t __ret;                                                           \
__asm__ ("lui %[ret], %%captab_hi(" X_STRINGIFY(Sym) ")\n"              \
         "daddiu %[ret], %[ret], %%captab_lo(" X_STRINGIFY(Sym) ")\n"   \
: [ret]"=r"(__ret) ::);     \
__ret;})

#define get_sym_call_captable_offset32(Sym) (uint32_t)({                \
uint32_t __ret;                                                         \
__asm__ ("lui %[ret], %%capcall_hi(" X_STRINGIFY(Sym) ")\n"              \
         "daddiu %[ret], %[ret], %%capcall_lo(" X_STRINGIFY(Sym) ")\n"   \
: [ret]"=r"(__ret) ::);     \
__ret;})

// There is currently no TLS captab_hi/lo so we are forced to extract the bits from a clcbi

#define get_tls_sym_captable_ndx16(Sym) (uint16_t)({                \
uint16_t __ret;                                                     \
__asm__ (".weak " X_STRINGIFY(Sym) "\n"                             \
         ".hidden "  X_STRINGIFY(Sym) "\n"                          \
         "clcbi $c1, %%captab_tls20(" X_STRINGIFY(Sym) ")($c26)\n"  \
         "cgetpcc $c1 \n"                                           \
         "clh %[ret], $zero, -2($c1) \n"                            \
: [ret]"=r"(__ret) ::"$c1");     \
__ret;})

#define get_unsafe_stack_reg() cheri_getreg(10)
#define get_safe_stack_reg() cheri_getreg(11)
#define get_return_reg() cheri_getreg(17)
#define get_function_start_reg() cheri_getreg(12)
#define get_return_data_reg() cheri_getreg(18)

#endif //CHERIOS_DYLINK_PLATFORM_H
