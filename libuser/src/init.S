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
#include "asm.S"
#include "syscalls.h"
#include "dylink.h"

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
# c6    : code_write_cap (for compaction)                                                                               #
# s1    : tls_segment_offset                                                                                            #
# a2    : data_seg_offset                                                                                               #
# a3    : data_seg_vaddr                                                                                                #
# a4    : code_seg_offset                                                                                               #
# a5    : code_seg_vaddr                                                                                                #
# a6    : tls_seg_vaddr                                                                                                 #
# s3    : the size of the tls prototype segment                                                                         #
# s5    : the size of the tls segment                                                                                   #
# s2    : startup_args                                                                                                  #
#                                                                                                                       #
# c9    : auth (null if unsecure, otherwise our auth cap                                                                #
# c7    : domain type reservation (may be null, be always have one if secure loaded)                                    #
# These fields are setup by act_register itself. Although the queue is an argument to the function                      #
#                                                                                                                       #
# c20   : queue                                                                                                         #
# c21   : self control reference                                                                                        #
# c23   : namespace reference (may be null for init and namespace)                                                      #
# c24   : kernel interface table                                                                                        #
#########################################################################################################################

	# Save arguments

	cincoffset  $c11, $c11, -(8*CAP_SIZE)
	move		$s0, $a0
    csc         $c3, $zero, 0($c11)
    csc         $c19, $zero, (CAP_SIZE*1)($c11)
    csc         $c20, $zero, (CAP_SIZE*2)($c11)
    csc         $c21, $zero, (CAP_SIZE*3)($c11)
    csc         $c22, $zero, (CAP_SIZE*4)($c11)
    csc         $c6,  $zero, (CAP_SIZE*5)($c11)
    csc         $c9,  $zero, (CAP_SIZE*6)($c11)
    csc         $c7,  $zero, (CAP_SIZE*7)($c11)

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
    dsubu       $s6, $t0, $a6
    clc         $c26, $s1, 0($c4)
    cincoffset  $c26, $c26, $s6                  #   Locals (unbounded)


    cscbi       $c7, CTLP_OFFSET_CDS($idc)      # store our sealing type

    dla		    $t0, crt_init_new_globals
    dsubu       $t0, $t0, $a5
    clc         $c13, $a4, 0($c4)               #   $c13 is data_seg cap
    cincoffset  $c12, $c13, $t0                 # crt_init_new_globals

    # These are in the global address space
    dla         $t1, __start___cap_relocs
    dla         $t2, __stop___cap_relocs
    # Now relative to the code segment
    dsubu       $t1, $t1, $a5
    dsubu       $t2, $t2, $a5

    cmove       $c19, $c4                       #   segment_table
    cincoffset  $c20, $c13, $t1                 #   start_relocs
    cincoffset  $c21, $c13, $t2                 #   stop_relocs
    cmove       $c22, $c5                       #   tls_segment_prototype
    move        $s4, $a4                        #   code_seg_offset

    # crt_init_new_globals(capability* segment_table, struct capreloc* start, struct capreloc* end)
    cmove       $c3, $c19
    cmove       $c4, $c20
    cmove       $c5, $c21
    call_func_reg

    # Now we can call functions normally - we can also load globals.

    get_sym     $c25, __cap_table_start         # Bounded cap tab
    get_sym     $c13, __cap_table_local_start   # This is the prototype for cap_table_local
    cgetlen     $t0, $c13
    csetboundsexact  $c26, $c26, $t0                 # bounds for local cap tab are same as prototype

    # Store these for future use
    get_sym     $c3, crt_code_seg_offset
    csd         $s4, $zero, 0($c3)
    get_sym     $c3, crt_tls_seg_size
    csd         $s5, $zero, 0($c3)
    get_sym     $c3, crt_cap_tab_local_addr
    csd         $s6, $zero, 0($c3)              # the seg relative addr of the cap table
    get_sym     $c3, crt_tls_proto
    csc         $c22, $zero, 0($c3)             # tls proto
    get_sym     $c3, crt_tls_proto_size
    csd         $s3, $zero, 0($c3)
    get_sym     $c3, crt_tls_seg_off
    csd         $s1, $zero, 0($c3)
    clc         $c4,  $zero, (CAP_SIZE*6)($c11)
    get_sym     $c3, own_auth
    csc         $c4, $zero, 0($c3)              # should prolly pass to object init rather than just setting here. But what the hell.

    # Now we have processed any relocations that might target the prototype - we can copy from proto
    clc         $c3, $s1, 0($c19)               # dest = tls_seg
    cmove       $c4, $c22                       # src = proto_tls
    move        $a0, $s3                        # length = length(proto_tls)
    call_func   memcpy

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
    clc $c9, $zero, (CAP_SIZE*7)($c11)          # cdl tres
    move        $a0, $s2                        # flags

    call_func   libuser_init


.set remaining_size, (40 * 4) # Complete guess. The linker refuses to do a symbol subtraction so this is what you get
    # Correctly bound this function. We will reach here with a global pcc
    cgetpcc     $c12
    cincoffset  $c12, $c12, (4 * 4)
    csetbounds  $c12, $c12, ((remaining_size))
    cjr         $c12
    nop

    # Call compact (will not do anything depending on flags. Easier to put condition in C)

    cmove   $c3, $c19                           # segment_table
    get_sym $c4, __start___cap_relocs           # start
    get_sym_val $t0, cap_relocs_size, d, $c5
    cincoffset $c5, $c4, $t0                    # end

    clc     $c6, $zero, (CAP_SIZE*5)($c11)      # code_write
    move    $a0, $s4                            # code_seg_offset
    move    $a1, $s2                            # flags
    clcbi   $c7, %capcall20(compact_code)($c25) # target

    call_func compact_code_tramp

    move        $a0, $s2                        # Startup flags
    li          $a1, 1                          # first thread
    call_func object_init_post_compact
	# Restore arguments
	move		$a0, $s0
	clc		    $c3, $zero, 0($c11)

# FIXME: This is bad! if our stack is not precise we will steal bytes not meant for us
	# Reclaim our stack frame and any stack passed arguments needed for init
	cgetlen     $t0, $c11
	csetoffset  $c11, $c11, $t0

	# Call main().
	call_func   main

	# main() done. Go to msg_entry if msg_enable, call object_destroy otherwise
	get_tls_sym_val	$t0, msg_enable, d, $c17
	beqz    $t0, make_call
    load_func object_destroy

    li      $a0, 0
    load_func msg_entry

make_call:
    call_func_reg

.end start_common

# This really HAS to deduplicate, so make sure it touches no globals. It allows compact_code to modify where it returns
.global compact_code_tramp
.ent compact_code_tramp


# capability compact_code(capability* segment_table, struct capreloc* start, struct capreloc* end,
#                        char* code_seg_write, size_t code_seg_offset, startup_flags_e flags, capability ret)

# trampoline accepts a target instead of ret, it jumps to the target and makes the ret argument
# on return it jumps to the value returned by the compact_code function

compact_code_tramp:

cmove $c12, $c7
cmove $c7, $c17
call_func_reg
cjr   $c3
nop

.end compact_code_tramp