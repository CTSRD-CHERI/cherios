#-
# Copyright (c) 2017 Hongyan Xia
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

# This is the function prototype
# register_t dcall_4(int method_nb, register_t rarg1, register_t rarg2,
#         register_t rarg3, register_t rarg4, register_t addr, register_t base);

    .text
    .global dcall_4
    .ent dcall_4
dcall_4:
    daddiu  $sp, $sp, -8
    dla     $t0, act_self_temp_stack
    dadd    $t0, $s4, $t0
    sd      $sp, 0($t0)
    sd      $ra, 0($sp)
    move    $v0, $a0
    move    $a0, $a1
    move    $a1, $a2
    move    $a2, $a3
    ld      $a3, 8($sp)
    move    $t0, $s4
    ld      $t9, 16($sp)
    jalr    $t9
    ld      $s4, 24($sp)
    # return here
    dla     $t0, act_self_temp_stack
    dadd    $t0, $s4, $t0
    ld      $sp, 0($t0)
    ld      $ra, 0($sp)
    jr      $ra
    daddiu  $sp, $sp, 8

dcall_4_end:
    .size dcall_4, dcall_4_end - dcall_4
    .end dcall_4
