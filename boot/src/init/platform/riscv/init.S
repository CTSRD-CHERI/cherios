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

#include "asm.S"
#include "reg_abi.h"

START_FUNC start

// A few saved registers that don't collide with anything init is passed
#define free_save   c27
#define free_save2  c26

    /* save these before they get clobbered */
    cmove           free_save,  abi_carg
    cmove           free_save2, abi_nano_req_auth

    /* Load statically allocated stack */
    la_relative     t0, __start_init_stack
    cspecialr       abi_stack, ddc
    cincoffset      abi_stack, abi_stack, t0
    la_relative     t0, __size_init_stack
    csetboundsexact  abi_stack, abi_stack, t0
    cincoffset      abi_stack, abi_stack, t0

    /* Init globals */
    call_func_early crt_init_globals_init

    /* Save nano auth */
    get_sym         a0, nanoreq_auth
    csc             free_save2, 0(ca0)

    /* Call libuser init (some of these are null due to init being quite an early task */
    cmove           ca0, abi_self_ref
    cmove           ca1, cnull
    cmove           ca2, abi_kernel_if
    cmove           ca3, abi_msg_queue
    cmove           ca4, cnull
    cmove           ca5, cnull
    cmove           ca6, cnull
    li              a7, 0
/* void libuser_init(act_control_kt self_ctrl,
				  act_kt ns_ref,
				  kernel_if_t* kernel_if_c,
				  queue_t * queue,
				  capability proc,
				  mop_t mop,
				  tres_t cds_res,
				  startup_flags_e flags)
*/
    call_func libuser_init

    /* Call main (slightly non-standard arguments) */
    /* int main(init_info_t * init_info, capability pool_auth_cap) */
    cmove           ca0, free_save
    auipcc          ca1, 0
    csetoffset      ca1, ca1, zero
    call_func       main

END_FUNC start
