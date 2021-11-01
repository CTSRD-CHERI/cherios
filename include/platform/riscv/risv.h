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

#ifndef CHERIOS_RISV_H
#define CHERIOS_RISV_H

#define CAN_SEAL_ANY 1

#define REG_SIZE    8
#define REG_SIZE_BITS 3

// TODO RISCV check these, I am just assuming they are the same as mips so copied them from there
#define _CHERI128_
#define U_PERM_BITS 4
#define CAP_SIZE 0x10
#define CAP_SIZE_S "0x10"
#define CAP_SIZE_BITS 4
#define BOTTOM_PRECISION 14
#define SMALL_PRECISION (BOTTOM_PRECISION - 1)
#define LARGE_PRECISION	(BOTTOM_PRECISION - 4)
#define SMALL_OBJECT_THRESHOLD  (1 << (SMALL_PRECISION)) // Can set bounds with byte align on objects less than this

/*
 * 64-bit RISCV types.
 */

typedef unsigned long	register_t;		/* 64-bit register */
typedef unsigned long	paddr_t;		/* Physical address */
typedef unsigned long	vaddr_t;		/* Virtual address */

typedef long		ssize_t;
typedef	unsigned long	size_t;

typedef long		off_t;

// TODO RISCV everything below is a dummy to get things to compile

#define STORE_IDC_INDEX(index, value)

typedef struct reg_frame {

} reg_frame_t;

#endif //CHERIOS_RISV_H
