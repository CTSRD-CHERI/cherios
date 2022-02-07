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

#ifndef CHERIOS_NANOIF_PLATFORM_H
#define CHERIOS_NANOIF_PLATFORM_H

#define NANO_KERNEL_IF_RAW_LIST_PLATFORM(ITEM, ...)     \
/* Create a new founded code block. The entry returned will be at offset entry0. */\
/* A public foundation has no associated data, but the entries can be converted into readable capabilities. */                  \
    ITEM(foundation_create_need_pad, entry_t,                                                                                   \
    (res_t, res, size_t, image_size, capability, image, size_t, entry0, size_t, n_entries, register_t, is_public, capability*, pad),  \
    __VA_ARGS__)

#define switch_regs         capability, ca0, capability, ca1, capability, ca2, capability, ca3, capability, ca4, capability, ca5, capability, ca6, capability, ca7, capability, ct0, capability, ct1, capability, ct2
#define ACT_ARG_LIST_NULL   NULL, NULL, NULL, NULL,  NULL, NULL, NULL, NULL, NULL, NULL, NULL

#endif //CHERIOS_NANOIF_PLATFORM_H
