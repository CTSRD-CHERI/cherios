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

#ifndef CHERIOS_USERNANO_H
#define CHERIOS_USERNANO_H

#include "nano/nanokernel.h"
#include "cheric.h"

static inline void init_nano_if_sys(if_req_auth_t auth) {
    nano_kernel_if_t interface;
    size_t limit = N_NANO_CALLS;
    capability data;
    __asm__(
    "li a0, 0                       \n"
    "li a1, 0                       \n"
    "cmove  ca2, %[auth]            \n"
    "cmove  ct0, %[ifc]             \n"
    "1:scall                        \n"
    "csc  ca3, 0(ct0)               \n"
    "addi a1, a1, 1                 \n"
    "cincoffset ct0, ct0, " CAP_SIZE_S "\n"
    "bne a1, %[total], 1b           \n"
    "cmove  %[d], ca4               \n"
    : [d]"=C"(data)
    : [ifc]"C"(&interface),[total]"r"(limit), [auth]"C"(auth)
    : "ca0", "ca1", "ca2", "ca3", "ca4", "ct0"
    );

    init_nano_kernel_if_t(&interface, data, NULL);
}

// TODO RISCV

#endif //CHERIOS_USERNANO_H
