#-
# Copyright (c) 2016 Robert N. M. Watson
# Copyright (c) 2016 Hadrien Barral
# Copyright (c) 2017 Lawrence Esswood
# All rights reserved.
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
# ("CTSRD"), as part of the DARPA CRASH research programme.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

.set noreorder
.set nobopt
.set noat

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
.include "asm.S"
#include "syscalls.h"

# Assembly stub on the way to running C code.  Assumes that $c0 and $pcc are
# global and suitable.  Jump to various startup functions to initialise the C
# runtime, and then call main().

.type act_self_ctrl, "tls_object"
.type msg_enable, "tls_object"

	.text
	.global start_common
	.ent start_common
start_common:

#########################################################################################################################
# Some "documentation" for the interface between the kernel and activation start                                        #
# These fields are setup by the caller of act_register                                                                  #
#                                                                                                                       #
# a0    : user GP argument (goes to main)                                                                               #
# c3    : user Cap argument (goes to main)                                                                              #
# c10   : unsafe stack                                                                                                  #
# c11   : stack                                                                                                         #
# c19   : MOP                                                                                                           #
# c22   : process reference                                                                                             #
#                                                                                                                       #
#                                                                                                                       #
# c4    : segment table                                                                                                 #
# c5    : tls_segment_prototype                                                                                         #
# c6    : code_write_cap (for plt stubs)                                                                                #
# s1    : tls_segment_offset                                                                                            #
# a2    : data_seg_offset                                                                                               #
# a3    : data_seg_vaddr                                                                                                #
# a4    : code_seg_offset                                                                                               #
# a5    : code_seg_vaddr                                                                                                #
# a6    : tls_seg_vaddr                                                                                                 #
                                                                                                                        #
# These fields are setup by act_register itself. Although the queue is an argument to the function                      #
#                                                                                                                       #
# c20   : queue                                                                                                         #
# c21   : self control reference                                                                                        #
# c23   : namespace reference (may be null for init and namespace)                                                      #
# c24   : kernel interface table                                                                                        #
#########################################################################################################################

	# Save arguments
	cincoffset  $c11, $c11, -(6*CAP_SIZE)
	move		$s0, $a0
    csc         $c3, $zero, 0($c11)
    csc         $c19, $zero, (CAP_SIZE*1)($c11)
    csc         $c20, $zero, (CAP_SIZE*2)($c11)
    csc         $c21, $zero, (CAP_SIZE*3)($c11)
    csc         $c22, $zero, (CAP_SIZE*4)($c11)
    csc         $c6,  $zero, (CAP_SIZE*5)($c11)
	# Initialise BSS and globals before running C constructors, so that C
	# constructors can take them (and other aspects of the conventional C
	# runtime) for granted.

    # We cant use captab yet - instead use dla and subtract vaddr of segment start, then derive from the segment table
    # The constants we need are provided by the linker

    clc         $c13, $a2, 0($c4)               #   $c13 is data_seg cap

    dla         $t0, __cap_table_start
    dsubu       $t0, $t0, $a3
    cincoffset  $c25, $c13, $t0                 #   Globals (unbounded)


    dla         $t0, __cap_table_local_start
    dsubu       $t0, $t0, $a6
    clc         $c26, $s1, 0($c4)
    cincoffset  $c26, $c26, $t0                  #   Locals (unbounded)

    dla		    $t0, crt_init_new_globals
    dsubu       $t0, $t0, $a5
    clc         $c12, $a4, 0($c4)
    cincoffset  $c12, $c12, $t0                 # crt_init_new_globals

    # These are in the global address space
    dla         $t1, __start___cap_relocs
    dla         $t2, __stop___cap_relocs
    # Now relative to the data segment
    dsubu       $t1, $t1, $a3
    dsubu       $t2, $t2, $a3

    cmove       $c19, $c4                       #   segment_table
    cincoffset  $c20, $c13, $t1                 #   start_relocs
    cincoffset  $c21, $c13, $t2                 #   stop_relocs
    cmove       $c22, $c5                       #   tls_segment_prototype

    # crt_init_new_globals(capability* segment_table, struct capreloc* start, struct capreloc* end)
    cmove       $c3, $c19
    cmove       $c4, $c20
    cmove       $c5, $c21
    call_func_reg

    # Now we can call functions normally - we can also load globals.

    get_sym     $c25, __cap_table_start         # Bounded cap tab
    get_sym     $c13, __cap_table_local_start   # This is the prototype for cap_table_local
    cgetlen     $t0, $c13
    csetbounds  $c26, $c26, $t0                 # bounds for local cap tab are same as prototype

    # Now we have processed any relocations that might target the prototype - we can copy from proto
    clc         $c3, $s1, 0($c19)               # dest = tls_seg
    cmove       $c4, $c22                       # src = proto_tls
    cgetlen     $a0, $c4                        # length = length(proto_tls)
    call_func   memcpy_c

    # Now we process relocations that target our TLS (must happen after the proto copy)

    # crt_init_new_locals(capability* segment_table, struct capreloc* start, struct capreloc* end) {

    cmove       $c3, $c19
    cmove       $c4, $c20
    cmove       $c5, $c21
    call_func   crt_init_new_locals


    call_func   crt_init_bss
    call_func   crt_call_constructors # FIXME wont work, pcc wont cover enough

	# Call object init.
	clc $c3, $zero, (CAP_SIZE*3)($c11)          # self_ctrl
	cmove		$c4, $c23                       # ns_ref
	cmove		$c5, $c24                       # kernel_if
	clc $c6, $zero, (CAP_SIZE*2)($c11)          # queue
	clc $c7, $zero, (CAP_SIZE*4)($c11)          # process ref
    clc $c8, $zero, (CAP_SIZE*1)($c11)          # mop
    clc $c9, $zero, (CAP_SIZE*5)($c11)          # plt_auth

    call_func   libuser_init

	# Restore arguments
	move		$a0, $s0
	clc		    $c3, $zero, 0($c11)
	cincoffset  $c11, $c11, (6*CAP_SIZE)

	# Call main().
	call_func   main

	# main() done. Go to msg_entry if msg_enable, call terminate otherwise
	get_tls_sym_val	$t0, msg_enable, d, $c17
	bne		$t0, $zero, msg_entry
	li      $a0, 0
	get_tls_sym_val $c2, act_self_ctrl, c, $c17
	cmove   $c3, $c2

	get_sym_val_array $c1, kernel_if, c, $c17, $zero, syscall_act_terminate_offset
	# We don't bother setting up a return as this will terminate us
	ccall   $c1, $c2, 2
	nop