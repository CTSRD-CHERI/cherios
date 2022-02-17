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

#ifndef CHERIOS_NANO_REG_LIST_H
#define CHERIOS_NANO_REG_LIST_H

// Format: ITEM(Name, WriteMask, __VA_ARGS__)
// Will generate an enum with all names
// Will generate a data block with all WriteMask fields concatanated (sets register_mask_table_size)
// Will generate a code block with stubs for a modification

// TODO RISCV
#define NANO_REG_LIST(ITEM, ...)                                        \
    ITEM(sstatus, RISCV_STATUS_SIE, __VA_ARGS__)                        \
    ITEM(sie, RISCV_MIE_STIE, __VA_ARGS__)                              \
    ITEM(sip, RISCV_MIE_STIE, __VA_ARGS__)

#define REG_LIST_TO_ENUM_LIST(Name, Mask, X, ...) X(NANO_REG_SELECT_ ## Name)
#define NANO_REG_LIST_FOR_ENUM(ITEM) NANO_REG_LIST(REG_LIST_TO_ENUM_LIST, ITEM)

#endif //CHERIOS_NANO_REG_LIST_H
