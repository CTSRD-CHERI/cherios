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

#define __ASSEMBLY__ 1
#include "cheric.h"

# Calls 'macro elem, index, global' for each 'elem' of the input list
.macro foreachi macro, index, global, head, tail:vararg
	\macro \head, \index, \global
	.ifnb  \tail
	foreachi \macro, "(\index+1)", \global, \tail
	.endif
.endm

# Does for i in range(start, stop, step) { macro(global, i) }
.macro for_range macro, global, start, stop, step
    .if \start != \stop
        \macro \global, \start
        for_range \macro, \global, (\start+\step), \stop, \step
    .endif
.endm


.macro START_LOCALS align
    .align \align
    locals_start:
    .set local_ctr, 0
.endm

.macro LOCAL_AT name
    .set \name, (local_ctr) # Allows us to get more space with an immediate
.endm

# Manually does alignment so we can keep track of offsets
.macro local_align align
    .set align_pow, (1 << \align)
    .set align_mask, align_pow - 1
    .set align_off, (align_pow - (local_ctr & align_mask)) & align_mask
    .if align_off != 0
        .space align_off
    .endif
    .set local_ctr, local_ctr + align_off
.endm


.macro local_var name, size, align
    local_align \align
    \name\()_label :
    LOCAL_AT \name
    .space \size
    .set local_ctr, local_ctr + \size
.endm

.macro END_LOCALS
    locals_end:
    .set locals_size, local_ctr
.endm

.macro local_reg_var name
    local_var \name, REG_SIZE, REG_SIZE_BITS
.endm

.macro local_cap_var name
    local_var \name, CAP_SIZE, CAP_SIZE_BITS
.endm

.macro local_cap_var_array, name, n
    local_var \name, (CAP_SIZE * \num), CAP_SIZE_BITS
.endm

.macro local_reg_var_array, name, num
    local_var \name, (REG_SIZE * \num), REG_SIZE_BITS
.endm

.macro local_array_start name, align, len
    local_align \align
    \name\()_start_label :
    .set \name, local_ctr
.endm

.macro local_array_end name, align, len
    local_align \align
    .set \name\()_size, local_ctr - \name
    .set \name\()_total_size, ((\len) * \name\()_size)
    .set extra_size, ((\len - 1) * \name\()_size)
    .set local_ctr, local_ctr + extra_size
    .space extra_size
.endm
