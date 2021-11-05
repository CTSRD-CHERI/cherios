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

#include "thread.h"

#define IDC_OFF 0
#define CGP_OFF (4 * CAP_SIZE)
#define C11_OFF (5 * CAP_SIZE)
#define C10_OFF (6 * CAP_SIZE)

#define START_OFF_SEC (7 * CAP_SIZE)
#define CARG_OFF_SEC (8 * CAP_SIZE)
#define ARG_OFF_SEC (9 * CAP_SIZE)

#define SPIN_OFF ((9 * CAP_SIZE) +  REG_SIZE)
#define SEG_TBL_OFF (10 * CAP_SIZE)
#define DATA_ARGS_OFF_SEC (SEG_TBL_OFF + (MAX_SEGS * CAP_SIZE))

_Static_assert((offsetof(struct secure_start_t, once)) == SPIN_OFF, "used by assembly below");
_Static_assert((offsetof(struct secure_start_t, segment_table)) == SEG_TBL_OFF, "used by assembly below");
_Static_assert((offsetof(struct secure_start_t, data_args)) == DATA_ARGS_OFF_SEC, "used by assembly below");

#define START_OFF       0
#define DATA_ARGS_OFF   CAP_SIZE


#define STRFY(X) #X
#define HELP(X) STRFY(X)

_Static_assert((offsetof(struct start_stack_args, start)) == START_OFF, "used by assembly below");
_Static_assert((offsetof(struct start_stack_args, data_args)) == DATA_ARGS_OFF, "used by assembly below");

#define LOAD_VADDR(reg, offset_reg) \
    "dsrl        " reg ", " offset_reg ", (" HELP(CAP_SIZE_BITS) " - " HELP(REG_SIZE_BITS) ") \n" \
    "cld         " reg ", " reg ", (" HELP(MAX_SEGS)" * " HELP(CAP_SIZE) ")($c4) \n"


// data_args = c3, segment_table = c4, tls_segment_proto = c5, tls_segment_off = a0,
// queue = c6, self_ctrl = c7, flags = a1, if_c = c8


// These will be called instead of normal init for new threads
// Check init.S in libuser for convention. Most relocations will have been processed - but we need to do locals again
// This trampoline constructs a locals and globals captable and then moves straight into c
__asm__ (
SANE_ASM
".text\n"
".global thread_start           \n"
".ent thread_start              \n"
"thread_start:                  \n"
"clc         $c13, $a2, 0($c4)  \n"
LOAD_VADDR("$a6", "$s1")
LOAD_VADDR("$a3", "$a2")
// Get globals
cheri_dla_asm("$t0", "__cap_table_start")
"dsubu       $t0, $t0, $a3                              \n"
"cincoffset  $c25, $c13, $t0                            \n"
"clcbi       $c25, %captab20(__cap_table_start)($c25)   \n"
// Get locals
cheri_dla_asm("$t0", "__cap_table_local_start")
"dsubu       $t0, $t0, $a6                  \n"
"clc         $c26, $s1, 0($c4)              \n"
"cincoffset  $c26, $c26, $t0                \n"
"clcbi       $c13, %captab20(__cap_table_local_start)($c25) \n"
"cgetlen     $t0, $c13                      \n"
"csetbounds  $c26, $c26, $t0                \n"

// Save c3 and a0

"move       $s0, $a0    \n"
"cmove      $c19, $c3   \n"

// c4 already segment_table
// c5 already tls_prototype
"clcbi   $c12, %capcall20(c_thread_start)($c25)\n"
"cincoffset $c3, $c11, " HELP(DATA_ARGS_OFF) "\n" // data_args
"move       $a0, $s1    \n"     // tls_segment
"cmove      $c6, $c20   \n"     // queue
"cmove      $c7, $c21   \n"     // self ctrl
"cmove      $c8, $c24   \n"     // kernel_if_t
"move       $a1, $s2    \n"     // startup flags
// Call c land now globals are set up
"cjalr      $c12, $c17  \n"
"cmove      $c18, $idc  \n"
// Reset stack then finish calling start with restored arguments
"cmove      $c12, $c3           \n"
"clc        $c4, $zero, " HELP(START_OFF) "($c11)\n"
"cmove      $c3, $c19           \n"
"move       $s0, $a0            \n"
"cmove      $c5, $cnull         \n"
"cgetlen    $at, $c11           \n"
"cjalr      $c12, $c17  \n"
"csetoffset $c11, $c11, $at     \n"
".end thread_start"
);

__asm__ (
SANE_ASM
".text\n"
".global secure_thread_start\n"
".ent secure_thread_start\n"
"secure_thread_start:\n"
// Must have provided an invocation
"cbtu       $idc, fail\n"
"cincoffset $c14, $idc, " HELP(SPIN_OFF) "\n"
// Obtain lock or fail
"li         $t0, 1\n"
"1: cllb    $t1, $c14\n"
"bnez       $t1, fail\n"
"cscb       $t1, $t0, $c14\n"
"beqz       $t1, 1b\n"
"cmove      $c19, $idc\n"
// Load stacks(s)
"clc        $c11, $zero, (" HELP(C11_OFF) ")($c19)\n"
"clc        $c10, $zero, (" HELP(C10_OFF) ")($c19)\n"
// Load globals
"clc        $c25, $zero, (" HELP(CGP_OFF) ")($c19)\n"
// Load idc (at this point we will take exceptions as the caller intended)
"clc        $idc, $zero, (" HELP(IDC_OFF) ")($c19)\n"
// Now call the same thread_start func
"clcbi      $c14, %captab20(crt_tls_seg_off)($c25)\n" // tls seg offset ptr
"cld        $a0, $zero, 0($c14)\n"                  // tls seg offset
"cincoffset $c4 , $c19, (" HELP(SEG_TBL_OFF) ")\n" // seg table
"csetbounds $c4, $c4," HELP(CAP_SIZE * MAX_SEGS) "\n" // and bound it
"clcbi      $c5, %captab20(crt_tls_proto)($c25)\n" // tls_proto
"clc        $c5, $zero, 0($c5)\n"
"cmove      $c6, $c20\n" // queue (make a new one straight away?)
"cmove      $c7, $c21\n" // self ctrl
"clcbi   $c12, %capcall20(c_thread_start)($c25)\n"
"cmove      $c8, $c24   \n"     // kernel_if_t
"cincoffset $c3 , $c19, (" HELP(DATA_ARGS_OFF_SEC) ")\n" // data arg table
"move       $a1, $s2    \n"    // startup flags
"cjalr      $c12, $c17  \n"
"cmove      $c18, $idc  \n"
// Finish calling start. Clean up will be done for us (argument c5)
"cmove      $c12, $c3           \n"
"cld        $a0 , $zero, (" HELP(ARG_OFF_SEC) ")($c19)\n" // arg
"clc        $c3 , $zero, (" HELP(CARG_OFF_SEC) ")($c19)\n" // carg
"clc        $c4 , $zero, (" HELP(START_OFF_SEC) ")($c19)\n" // start
"cjalr      $c12, $c17  \n"
"cmove      $c5, $c19    \n"        // Get callee to clean up this object
"fail:      teqi $zero, 0\n"
"nop\n"
".end secure_thread_start\n"
);
