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

#ifndef CHERIOS_CHERIPLT_PLATFORM_H
#define CHERIOS_CHERIPLT_PLATFORM_H

#define PLT_STUB_CGP_ONLY_CSD(name, obj, tls, tls_reg, alias, alias2)               \
__asm__ (                                                                           \
    ".text\n"                                                                       \
    ".global " #name "\n"                                                           \
    X_STRINGIFY(ASM_VISIBILITY) " " #name "\n"                                      \
    ".type " #name ", \"function\"\n"                                               \
    "" #name ":\n"                                                                  \
    alias                                                                           \
    WEAK_DUMMY(name)                                                                \
    "lui t2, %captab_call_hi(" #name "_dummy)\n"                                    \
    "cincoffset "X_STRINGIFY(PLT_REG_TARGET)", " X_STRINGIFY(PLT_REG_GLOB) ", t2\n" \
    "clc " X_STRINGIFY(PLT_REG_TARGET) ", %captab_call_lo(" #name "_dummy)(" X_STRINGIFY(PLT_REG_TARGET) ")\n" \
    "lui t2, %captab" tls "_hi(" EVAL1(STRINGIFY(obj)) ")\n"                        \
    "cincoffset ct0, " tls_reg ", t2\n"                                             \
    "clc ct0, %captab" tls "_lo(" EVAL1(STRINGIFY(obj)) ")(ct0)\n"                  \
    "cmove  ct5, ct6\n"                                                             \
    "cinvoke " X_STRINGIFY(PLT_REG_TARGET) ", ct0\n"                                \
    alias2                                                                          \
    ".size " #name ", 32\n"                                                         \
);

#define PLT_STUB_CGP_ONLY_MODE_SEL(name, obj, tls, tls_reg, alias, alias2)          \
__asm__ (                                                                           \
    ".text\n"                                                                       \
    ".global " #name "\n"                                                           \
    X_STRINGIFY(ASM_VISIBILITY) " " #name "\n"                                      \
    ".type " #name ", \"function\"\n"                                               \
    "" #name ":\n"                                                                  \
    alias                                                                           \
    WEAK_DUMMY(name)                                                                \
    WEAK_DUMMY(obj)                                                                 \
    "lui t3, %captab_call_hi(" EVAL1(STRINGIFY(obj)) "_dummy)\n"                    \
    "cincoffset ct2, " X_STRINGIFY(PLT_REG_GLOB) ", t3\n"                           \
    "clc ct2, %captab_call_lo(" EVAL1(STRINGIFY(obj)) "_dummy)(ct2)\n"              \
    "lui t3, %captab_call_hi(" #name "_dummy)\n"                                    \
    "cincoffset "X_STRINGIFY(PLT_REG_TARGET)", " X_STRINGIFY(PLT_REG_GLOB) ", t3\n" \
    "clc " X_STRINGIFY(PLT_REG_TARGET) ", %captab_call_lo(" #name "_dummy)(" X_STRINGIFY(PLT_REG_TARGET) ")\n" \
    "lui t3, %captab" tls "_hi("EVAL1(STRINGIFY(obj))")\n"                           \
    "cincoffset "X_STRINGIFY(PLT_REG_TARGET_DATA)", " tls_reg ", t3\n"              \
    "clc " X_STRINGIFY(PLT_REG_TARGET_DATA) ", %captab" tls "_lo(" EVAL1(STRINGIFY(obj)) ")(" X_STRINGIFY(PLT_REG_TARGET_DATA) ")\n" \
    "cjr ct2\n"                                                                     \
    alias2                                                                          \
    ".size " #name ", 40\n"                                                         \
);

// Note: stores are encoded differently, so we can't use the captab relocations on them.
// Instead, we do it on an incoffset, and then use a zero immediate in the load

#define STORE_CAPTAB_HELPER(reloc, sym, base_reg, val)                              \
        __asm__ ("lui t0, %%captab" reloc "_hi(" sym ")\n"                          \
                 "cincoffset ct0, " base_reg ", t0\n"                               \
                 "cincoffset ct0, ct0, %%captab" reloc "_lo(" sym ")\n"             \
                 "csc        %[d], 0(ct0)\n"                                        \
                 ::[d]"C"(val) : "t0", "ct0"                                        \
                );

#define INIT_OBJ(name, ret, sig, ...) \
    STORE_CAPTAB_HELPER("_call", #name "_dummy", X_STRINGIFY(PLT_REG_GLOB), plt_if -> name)

#define PLT_STORE_CAPTAB(tls, type, reg, data) \
    STORE_CAPTAB_HELPER(tls, #type "_data_obj", reg, data)

#define PLT_STORE_CAPTAB_CALL(type, trust_mode) \
    STORE_CAPTAB_HELPER("_call", #type "_data_obj_dummy", X_STRINGIFY(PLT_REG_GLOB), trust_mode)

#define PLT_LOAD_CAPTAB_CALL(type, trust_mode) \
        __asm__ (DECLARE_WEAK_OBJ_DUMMY(type) "; clc %[d], %%captab_call_lo(" #type "_data_obj_dummy)("X_STRINGIFY(PLT_REG_GLOB)")\n":[d]"=C"(trust_mode)::)

#endif //CHERIOS_CHERIPLT_PLATFORM_H
