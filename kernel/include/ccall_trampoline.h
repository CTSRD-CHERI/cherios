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

/* Helper functions to make assembly trampolines for ccallable functions. Exposes a _get_trampoline for a function */

extern capability kernel_ccall_trampoline_c0;
void kernel_setup_trampoline(void);

#define DECLARE_TRAMPOLINE(F) capability F ## _get_trampoline(void)


#define DEFINE_TRAMPOLINE_EXTRA(F, EXTRA_B, EXTRA_A)            \
extern void F ## _trampoline(void);                                 \
capability F ## _get_trampoline(void) {                             \
    return (capability)(&(F ## _trampoline));                   \
}                                                               \
__asm__ (                                                       \
        ".text\n"                                               \
        ".global " #F "_trampoline\n"                           \
        #F "_trampoline:\n"                                     \
        "dla			$t0, kernel_ccall_stack_swap\n"         \
        "cgetpccsetoffset  $c16, $t0\n"                         \
        "cjalr			$c16, $c14\n"                           \
        "nop\n"                                                 \
        EXTRA_B                                                 \
        "dla            $t0, "#F"\n"                            \
        "cgetpccsetoffset  $c12, $t0\n"                         \
        "cjalr          $c12, $c17\n"                           \
        "nop\n"                                                 \
        EXTRA_A                                                 \
        "dla            $t0, kernel_ccall_stack_unswap\n"       \
        "jr				$t0\n"                                  \
        "nop\n"                                    				\
);

#define DEFINE_TRAMPOLINE(F) DEFINE_TRAMPOLINE_EXTRA(F,,)

#endif //CHERIOS_CCALL_TRAMPOLINE_H
