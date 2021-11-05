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

#if (DEBUG_COUNT_CALLS)

#if (IS_KERNEL)
#define GET_STATS   "clcbi   $c15, " X_STRINGIFY(CTLP_OFFSET_CGP) "($idc) \n"\
                    "cbtu $c15, 77f \n "\
                    "nop \n"\
                    "clcbi	$c15, %captab20(own_stats)($c15)\n"
#else
#define GET_STATS ".type own_stats, \"tls_object\"\n clcbi $c15, %captab_tls20(own_stats)($c26)\n"
#endif

#define BUMP_CSD_COUNTER GET_STATS \
        "clcbi   $c15, 0($c15) \n"\
        "cbtu    $c15, 77f\n"\
        "nop\n"\
        "cld     $t0, $zero, " X_STRINGIFY(STATS_COMMON_DOMAIN_OFFSET) "($c15)\n"\
        "daddiu  $t0, $t0, 1\n"\
        "csd     $t0, $zero, " X_STRINGIFY(STATS_COMMON_DOMAIN_OFFSET) "($c15)\n"\
        "77:\n"

#else
#define BUMP_CSD_COUNTER
#endif

#define PLT_STUB_CGP_ONLY_CSD(name, obj, tls, tls_reg, alias, alias2) \
__asm__ (                       \
    SANE_ASM                    \
    ".text\n"                   \
    ".p2align 3\n"              \
    ".global " #name "\n"       \
    ".ent " #name "\n"          \
    X_STRINGIFY(ASM_VISIBILITY) " " #name "\n"\
    "" #name ":\n"              \
    alias                       \
    WEAK_DUMMY(name)            \
    BUMP_CSD_COUNTER            \
    "clcbi       $c1, %capcall20(" #name "_dummy)($c25)\n"      \
    "clcbi       $c2, %captab" tls "20(" EVAL1(STRINGIFY(obj)) ")(" tls_reg ")\n"   \
    "ccall       $c1, $c2, 2 \n"\
    "nop\n"                     \
    alias2                      \
    ".end " #name "\n"          \
);

#define PLT_STUB_CGP_ONLY_MODE_SEL(name, obj, tls, tls_reg, alias, alias2) \
__asm__ (                       \
    SANE_ASM                    \
    ".text\n"                   \
    ".p2align 3\n"              \
    ".global " #name "\n"       \
    ".ent " #name "\n"          \
    X_STRINGIFY(ASM_VISIBILITY) " " #name "\n"\
    "" #name ":\n"              \
    alias                       \
    WEAK_DUMMY(name)            \
    WEAK_DUMMY(obj)             \
    "clcbi       $c1, %capcall20(" #name "_dummy)($c25)\n" \
    "clcbi       $c12,%capcall20(" EVAL1(STRINGIFY(obj)) "_dummy)($c25)\n"          \
    "cjr         $c12                                 \n"                           \
    "clcbi       $c2, %captab" tls "20(" EVAL1(STRINGIFY(obj)) ")(" tls_reg ")\n"   \
    alias2                      \
    ".end " #name "\n"          \
);

#define INIT_OBJ(name, ret, sig, ...)             \
        __asm__ ("cscbi %[d], %%capcall20(" #name "_dummy)($c25)\n"::[d]"C"(plt_if -> name):);

#define PLT_STORE_CAPTAB(tls, type, reg, data) \
        __asm__ ("cscbi %[d], %%captab" tls "20(" #type "_data_obj)(" reg ")\n"::[d]"C"(data):)
#define PLT_STORE_CAPTAB_CALL(type, trust_mode) \
        __asm__ (DECLARE_WEAK_OBJ_DUMMY(type) "; cscbi %[d], %%capcall20(" #type "_data_obj_dummy)($c25)\n"::[d]"C"(trust_mode):)
#define PLT_LOAD_CAPTAB_CALL(type, trust_mode) \
        __asm__ (DECLARE_WEAK_OBJ_DUMMY(type) "; clcbi %[d], %%capcall20(" #type "_data_obj_dummy)($c25)\n":[d]"=C"(trust_mode)::)

#endif //CHERIOS_CHERIPLT_PLATFORM_H
