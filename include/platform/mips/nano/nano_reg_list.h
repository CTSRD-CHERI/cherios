/*-
 * Copyright (c) 2018 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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
#ifndef CHERIOS_NANO_REG_LIST_H
#define CHERIOS_NANO_REG_LIST_H

// Format: ITEM(Name, RegNum, Selector, WriteMask, __VA_ARGS__)
// Will generate an enum with all names
// Will generate a data block with all WriteMask fields concatanated (sets register_mask_table_size)
// Will generate a code block with stubs for a modification

#define NANO_REG_LIST(ITEM, ...)                                                            \
    ITEM(COUNT, MIPS_CP0_REG_COUNT, 0, 0xFFFFFFFF, __VA_ARGS__)                             \
    ITEM(COMPARE, MIPS_CP0_REG_COMPARE, 0, 0xFFFFFFFF, __VA_ARGS__)                         \
    ITEM(STATUS, MIPS_CP0_REG_STATUS, 0, 0b00001000000000001000000000000111, __VA_ARGS__)   \
    ITEM(EBASE, $15, 1, 0, __VA_ARGS__)                                                     \
    ITEM(HWRENA, $7, 0, 0xFFFFFFFF, __VA_ARGS__)

#define REG_LIST_TO_ENUM_LIST(Name, Reg, Select, Mask, X, ...) X(NANO_REG_SELECT_ ## Name)
#define NANO_REG_LIST_FOR_ENUM(ITEM) NANO_REG_LIST(REG_LIST_TO_ENUM_LIST, ITEM)

#endif //CHERIOS_NANO_REG_LIST_H
