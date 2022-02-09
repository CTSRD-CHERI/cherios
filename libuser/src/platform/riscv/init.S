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
#include "elf.h"
#include "dylink.h"

    # (abi_seg_tbl holds caps, followed by vaddrs)
.macro load_vaddr dst, offset_reg, cap_tmp
    srli        \dst, \offset_reg, (CAP_SIZE_BITS - REG_SIZE_BITS)
    cincoffset  \cap_tmp, abi_seg_tbl, \dst
    cld         \dst, (MAX_SEGS * CAP_SIZE)(\cap_tmp)
.endm
.macro load_seg dst, offset_reg
    cincoffset  \dst, abi_seg_tbl, \offset_reg
    clc         \dst, 0(\dst)
.endm

.macro load_sym_early dst, rdst, sym, offset_reg, rtmp
    load_vaddr  \rdst, \offset_reg, \dst
    la_relative \rtmp, \sym
    sub         \rtmp, \rtmp, \rdst
    load_seg    \dst, \offset_reg
    cincoffset  \dst, \dst, \rtmp
.endm

#define LOAD_SYM_EARLY(dst, sym, off_reg, tmp) \
    load_sym_early dst, Reg_Cap_To_Int(dst), sym, off_reg, tmp

// Chosen to avoid cloberring registers from reg_abi.h

#define tmp_reg  t1

#define LOAD_SYM_CODE(dst, sym) LOAD_SYM_EARLY(dst, sym, abi_code_seg_offset, tmp_reg)
#define LOAD_SYM_DATA(dst, sym) LOAD_SYM_EARLY(dst, sym, abi_data_seg_offset, tmp_reg)
#define LOAD_SYM_TLS(dst, sym) LOAD_SYM_EARLY(dst, sym, abi_tls_seg_offset, tmp_reg)

START_FUNC start_common

    # Save arguments that would be clobbered (8/9 and  and 18-27 do not need saving)
    SPILL_STACK(abi_cert, abi_tls_proto, abi_nano_req_auth, abi_proc_ref, abi_carg, abi_arg, abi_tls_mem_size)

    # Load (currently unbounded) tables to bootstrap. We assume these symbols are in these segments, or rather,
    # The loader should provide the correct offsets to find these symbols.
    LOAD_SYM_DATA   (cgp, __cap_table_start)
    LOAD_SYM_TLS    (abi_local, __cap_table_local_start)

    # Store sealing type
    csc             abi_type_res, CTLP_OFFSET_CDS(abi_local)

    # Init globals
    # crt_init_new_globals(capability* segment_table, struct capreloc* start, struct capreloc* end)
    cmove           ca0, abi_seg_tbl
    cllc_rel        ca1, __start___cap_relocs
    cllc_rel        ca2, __stop___cap_relocs
    call_func_early crt_init_new_globals

    # From now on we can call functions / load globals properlly

    # Bound captabs
    GET_SYM             (ct0, __cap_table_start)
    cmove               cgp, ct0
    GET_SYM             (ct0, __cap_table_local_start) # Not the actual captable, will be to prototype
    cgetlen             t0, ct0 # but we can use it to set bounds properly on our local captab
    csetboundsexact     abi_local, abi_local, t0

    # Copy segment table vaddrs to permenant location
    GET_SYM             (ca0, crt_segment_table_vaddrs)
    cincoffset          ca1, abi_seg_tbl, (CAP_SIZE*MAX_SEGS)
    li                  a2, REG_SIZE * MAX_SEGS
    call_func           memcpy

    # TODO: Save a cap to the dynamic segment (needed for dyanmic libs to work)
    # Save a bunch of things for the CRT
    LOAD_SPILL          (ct1, abi_cert)
    GET_SYM             (ct0, own_auth)
    csc                 ct1, 0(ct0)
    GET_SYM             (ct0, crt_code_seg_offset)
    csd                 abi_code_seg_offset, 0(ct0)
    GET_SYM             (ct0, crt_tls_seg_size)
    LOAD_SPILL          (t1, abi_tls_mem_size)
    csd                 t1, 0(ct0)
    GET_SYM             (ct0, crt_cap_tab_local_addr)
    load_seg            ca0, abi_tls_seg_offset
    csub                t1, abi_local, ca0
    csd                 t1, 0(ct0)
    GET_SYM             (ct0, crt_tls_proto)
    LOAD_SPILL          (ca1, abi_tls_proto)
    csc                 ca1, 0(ct0)
    GET_SYM             (ct0, crt_tls_proto_size)
    move                a2, abi_tls_fil_size
    csd                 a2, 0(ct0)
    GET_SYM             (ct0, crt_tls_seg_off)
    csd                 abi_tls_seg_offset, 0(ct0)
    LOAD_SPILL          (ct1, abi_nano_req_auth)
    GET_SYM             (ct0, nanoreq_auth)
    csc                 ct1, 0(ct0)

    # Copy TLS prototype to this threads TLS (all of these arguments were setup as we saved things for the CRT
                       #ca0 = abi_tls_seg_offset
                       #ca1 = abi_tls_proto
                       #a2 = abi_tls_fil_size
    call_func           memcpy


    # Now we process relocations that target our TLS (must happen after the proto copy)
    # crt_init_new_locals(capability* segment_table, struct capreloc* start, struct capreloc* end) {

    cmove           ca0, abi_seg_tbl
    cllc_rel        ca1, __start___cap_relocs
    cllc_rel        ca2, __stop___cap_relocs
    call_func       crt_init_new_locals

    # Call C constructors.
    call_func           crt_call_constructors

    /* Call libuser init (some of these are null due to init being quite an early task */
    cmove           ca0, abi_self_ref
    cmove           ca1, abi_ns_ref
    cmove           ca2, abi_kernel_if
    cmove           ca3, abi_msg_queue
    LOAD_SPILL      (ca4, abi_proc_ref)
    cmove           ca5, abi_mop
    cmove           ca6, abi_type_res
    move            a7, abi_start_flags
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
# call compact TODO
    LOAD_SPILL      (a0, abi_arg)
    LOAD_SPILL      (ca1, abi_carg)
# Reset stack (anything saved on it we do not need anymore
    cgetlen         t0, csp
    csetoffset      csp, csp, t0
# Call main
    call_func       main
# main done. if msg_enable, go to msg_entry, otherwise destroy object
    load_func       object_destroy
    GET_TLS_SYM_VAL (t0, msg_enable)
    beqz            t0, end
    li              a0, -1  # infinite timeout
    li              a1, 0   # no flags
    load_func       msg_entry
    end:
    call_func_loaded
END_FUNC start_common

// Lightweight objects do not create extra threads
#ifndef LIGHTWEIGHT_OBJECT

#include "thread.h"

START_FUNC thread_start

    LOAD_SYM_DATA   (cgp, __cap_table_start)
    LOAD_SYM_TLS    (abi_local, __cap_table_local_start)


    // Call c_thread_start
    cmove           ca2, abi_tls_proto
    move            a3, abi_tls_seg_offset
    cmove           ca4, abi_msg_queue
    cmove           ca5, abi_self_ref
    move            a6, abi_start_flags
    cmove           ca7, abi_kernel_if
    // Save arguments quickly here
    move            s0, a0
    cmove           cs1, ca1
    // Keep calling c_thread_start
    cincoffset      ca0, csp, DATA_ARGS_OFF // Data args are stack passed
    cmove           ca1, abi_seg_tbl
    call_func       c_thread_start
/*
capability c_thread_start(capability* data_args, capability* segment_table, capability tls_segment_prototype, register_t tls_segment_offset,
                    queue_t* queue, act_control_kt self_ctrl, startup_flags_e flags,
                    kernel_if_t* kernel_if_c) {

return argument is the next function to call after stack recovery.
*/
    # Save target
    cmove           cra, ca0
    # Get last arugment off stack and restore arguments
    clc             ca2, START_OFF(csp)
    move            a0, s0
    cmove           ca1, cs1
    # Reset stack
    cgetlen         t0, csp
    csetoffset      csp, csp, t0
    # Call main
    cjalr           cra, cra

END_FUNC thread_start

START_FUNC secure_thread_start

    TODO_SPIN

END_FUNC secure_thread_start

#endif