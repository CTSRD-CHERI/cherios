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

#ifndef CHERIOS_REG_ABI_H
#define CHERIOS_REG_ABI_H

// While programs are running

#define abi_global              c3
#define abi_local               c31
#define abi_link                c1
#define abi_stack               c2
#define abi_unsafe              c4
#define abi_data_link           c0 // Not sure what this will be yet
// For programs main

#define abi_arg                 x10
#define abi_carg                c11

// For init (secure entry), supplied by loader
#define abi_secure_entry        c17

// For init (insecure entry, or trampoline from secure entry)
// supplied by kernel
#define abi_msg_queue           c20
#define abi_self_ref            c21
#define abi_ns_ref              c23
#define abi_kernel_if           c24
#define abi_nano_req_auth       c29

// supplied by loader
#define abi_mop                 c19
#define abi_seg_tbl             c18
#define abi_tls_proto           c13
#define abi_code_write          c14
#define abi_tls_seg_offset      x8
#define abi_data_seg_offset     x15
#define abi_code_seg_offset     x16
#define abi_tls_fil_size        x9
#define abi_tls_mem_size        x12
#define abi_dynamic_vaddr       x19
#define abi_dynamic_size        x20
#define abi_program_base        x5

// supplied by proc_manager
#define abi_found_enter         c6
#define abi_nano_if_data        c22
#define abi_type_res            c26
#define abi_cert                c27
#define abi_start_flags         x25
#define abi_proc_ref            c28

// Some wrappers to access members of a frame_t by a specific abi_reg
#define cf(abi_reg) __CONCAT(cf_, abi_ ## abi_reg)

#define cf_global           cf(global)
#define cf_stack            cf(stack)
#define cf_link             cf(link)
#define cf_arg              cf(arg)
#define cf_carg             cf(carg)
#define cf_idc              cf(local)

#define cf_msg_queue        cf(msg_queue)
#define cf_self_ref         cf(self_ref)
#define cf_ns_ref           cf(ns_ref)
#define cf_kernel_if        cf(kernel_if)
#define cf_nano_req_auth    cf(nano_req_auth)

#define cf_mop              cf(mop)
#define cf_seg_tbl          cf(seg_tbl)
#define cf_tls_proto        cf(tls_proto)
#define cf_code_write       cf(code_write)
#define cf_tls_seg_offset   cf(tls_seg_offset)
#define cf_data_seg_offset  cf(data_seg_offset)
#define cf_code_seg_offset  cf(code_seg_offset)
#define cf_tls_fil_size     cf(tls_fil_size)
#define cf_tls_mem_size     cf(tls_mem_size)
#define cf_dynamic_vaddr    cf(dynamic_vaddr)
#define cf_dynamic_size     cf(dynamic_size)

#define cf_found_enter      cf(found_enter)
#define cf_nano_if_data     cf(nano_if_data)
#define cf_type_res         cf(type_res)
#define cf_cert             cf(cert)
#define cf_start_flags      cf(start_flags)
#define cf_proc_ref         cf(proc_ref)
#define cf_secure_entry     cf(secure_entry)
#define cf_program_base     cf(program_base)

// This is ugly. For some reason init (and init only) has an extra argument
#define cf_init_tls_base    cf_x30

#endif //CHERIOS_REG_ABI_H
