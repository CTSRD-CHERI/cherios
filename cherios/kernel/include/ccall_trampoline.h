/*-
 * Copyright (c) 2016 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#ifndef CHERIOS_CCALL_TRAMPOLINE_H
#define CHERIOS_CCALL_TRAMPOLINE_H

#include "mips.h"

// stack layout for a sealed call - idc will contain the stack:

/**************/
/* ddc (c0)   */
/* call guard */ // (64 bits but padded to cap size) <-- stack points here
/**************/

// Then we save these as we are a trampoline:

/**************/
/* rddc       */
/* cra (c17)  */
/* crd (c18)  */
/**************/

/* Helper functions to make assembly trampolines for ccallable functions. Exposes a _get_trampoline for a function */
/* Eventually loading the stakc from idc and popping c0 will be a part of the calling convention */

// FIXME the activation sturct should really be seen as being passed on the stack as the first item

#define CALLER cheri_setoffset(get_idc(), 0)

#define EXPAND_F(F)

// TODO most of this trampoline can be removed by adding it before function prologs. All that stays is the guard and
// TODO Load of C0.

#define DEFINE_TRAMPOLINE_EXTRA(F, EXTRA_B, EXTRA_A)            \
extern void F ## _trampoline(void);                             \
__asm__ (                                                       \
        SANE_ASM                                                \
        ".text\n"                                               \
        ".global " #F "_trampoline\n"                           \
        #F "_trampoline:\n"                                     \
/*      Sets guard to 0 -> 1                                  */\
        "dli         $t3, 1 \n"                                 \
        "1: clld        $t2, $idc \n"                           \
        "tnei        $t2, 0 \n"                                 \
        "cscd        $t2, $t3, $idc\n"                          \
        "beqz        $t2, 1b\n"                                 \
        "nop \n"                                                \
/*      Swap ddc                                              */\
        "csc         $c0, $zero, (-1 * 32)($idc) \n"            \
        "clc         $c0, $zero, (1 * 32)($idc) \n"             \
/*      Trampoline                                            */\
        "csc         $c17, $zero, (-2 * 32)($idc) \n"           \
        "csc         $c18, $zero, (-3 * 32)($idc) \n"           \
        "cincoffset  $c11, $idc, -(3 * 32) \n"                  \
        EXTRA_B                                                 \
        "dla            $t0, "#F"\n"                            \
        "cgetpccsetoffset  $c12, $t0\n"                         \
        "cjalr          $c12, $c17\n"                           \
        "nop\n"                                                 \
        EXTRA_A                                                 \
/* on return from call restore as needed                      */\
        "clc         $c18, $zero, (0 * 32)($c11) \n"            \
        "clc         $c17, $zero, (1 * 32)($c11) \n"            \
        "clc         $c0, $zero, (2 * 32)($c11) \n"             \
/* Clear call guard                                           */\
        "sync \n"                                               \
        "csd         $zero, $zero, (3 * 32)($c11) \n"           \
/* Clear everything caller saved ($c1 to $c16 minus 3 (which is the return)) */\
        "cclearlo   (0b1111111111110110) \n"                    \
        "ccall $c17, $c18, 2 \n"                                \
        "nop\n"                                                 \
);

#endif //CHERIOS_CCALL_TRAMPOLINE_H
