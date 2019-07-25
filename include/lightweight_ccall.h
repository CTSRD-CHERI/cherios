/*-
 * Copyright (c) 2019 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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
#ifndef CHERIOS_LIGHTWEIGHT_CCALL_H
#define CHERIOS_LIGHTWEIGHT_CCALL_H


// Usage LIGHTWEIGHT_CCALL_FUNC([r|c|v], func_arg, data_arg, [0,4], (reg_args) ... , [0,4], (cap_args) ...)

#define LIGHTWEIGHT_CCALL_FUNC(OutT, f, d, ...) \
  LIGHTWEIGHT_CCALL_FUNC_BASE(OutT, f,d, LW_ARGS(__VA_ARGS__), LW_INS(__VA_ARGS__))






// Some helpers

#define LW_DEF_r register register_t reg_out __asm("v0");
#define LW_DEF_c register capability reg_out __asm("$c3");
#define LW_DEF_v
#define LW_DEF(X) LW_DEF_ ## X

#define LW_ASM_r "=r"(reg_out)
#define LW_ASM_c "=C"(reg_out)
#define LW_ASM_v
#define LW_ASM(X) LW_ASM_ ## X

#define LW_VAL_r reg_out;
#define LW_VAL_c reg_out;
#define LW_VAL_v
#define LW_VAl(X) LW_VAL_ ## X

#define LW_CLOB_r "a0", "$c3"
#define LW_CLOB_c "a0"
#define LW_CLOB_v "a0", "$c3"
#define LW_CLOB(X) LW_CLOB_ ## X


#define LW_DEFS_C0(...)
#define LW_DEFS_C1(A0, ...)             LW_DEFS_C0(__VA_ARGS__) register capability _c3 __asm ("$c3") = A0;
#define LW_DEFS_C2(A0, A1, ...)         LW_DEFS_C1(A0, __VA_ARGS__) register capability _c4 __asm ("$c4") = A1;
#define LW_DEFS_C3(A0, A1, A2, ...)     LW_DEFS_C2(A0, A1, __VA_ARGS__) register capability _c5 __asm ("$c5") = A2;
#define LW_DEFS_C4(A0, A1, A2, A3, ...) LW_DEFS_C3(A0, A1, A2, __VA_ARGS__) register capability _c6 __asm ("$c6") = A3;

#define LW_DEFS_R0(Cargs, ...)          LW_DEFS_C ## Cargs (__VA_ARGS__)
#define LW_DEFS_R1(A0, ...)             LW_DEFS_R0(__VA_ARGS__) register register_t _a0 __asm ("a0") = A0;
#define LW_DEFS_R2(A0, A1, ...)         LW_DEFS_R1(A0, __VA_ARGS__) register register_t _a1 __asm ("a1") = A1;
#define LW_DEFS_R3(A0, A1, A2, ...)     LW_DEFS_R2(A0, A1, __VA_ARGS__) register register_t _a2 __asm ("a2") = A2;
#define LW_DEFS_R4(A0, A1, A2, A3, ...) LW_DEFS_R3(A0, A1, A2, __VA_ARGS__) register register_t _a3 __asm ("a3") = A3;

#define LW_ARGS(Rargs, ...) LW_DEFS_R ## Rargs (__VA_ARGS__)

#define LW_INS_C0(...)
#define LW_INS_C1(A0, ...)             LW_INS_C0(__VA_ARGS__) , "C" (_c3)
#define LW_INS_C2(A0, A1, ...)         LW_INS_C1(A0, __VA_ARGS__) , "C" (_c4)
#define LW_INS_C3(A0, A1, A2, ...)     LW_INS_C2(A0, A1, __VA_ARGS__) , "C" (_c5)
#define LW_INS_C4(A0, A1, A2, A3, ...) LW_INS_C3(A0, A1, A2, __VA_ARGS__) , "C" (_c6)

#define LW_INS_R0(Cargs, ...)          LW_INS_C ## Cargs (__VA_ARGS__)
#define LW_INS_R1(A0, ...)             LW_INS_R0(__VA_ARGS__) ,"r" (_a0)
#define LW_INS_R2(A0, A1, ...)         LW_INS_R1(A0, __VA_ARGS__) ,"r" (_a1)
#define LW_INS_R3(A0, A1, A2, ...)     LW_INS_R2(A0, A1, __VA_ARGS__) ,"r" (_a2)
#define LW_INS_R4(A0, A1, A2, A3, ...) LW_INS_R3(A0, A1, A2, __VA_ARGS__) ,"r" (_a3)

#define LW_INS(Rargs, ...) LW_INS_R ## Rargs (__VA_ARGS__)

#define CCALL_SLOTLESS   "ccall      $c1, $c2, 2       \n nop\n"

#define LIGHTWEIGHT_CCALL_FUNC_BASE(OutT, f, d, EXTRA_DEFS, EXTRA_IN)               \
({                                                                                  \
  LW_DEF(OutT)                                                                      \
  register capability _code_tmp __asm ("$c1") = f;                                  \
  register capability _data_tmp __asm ("$c2") = d;                                  \
  register capability _data_link __asm ("$c18") = get_ctl();                        \
  EXTRA_DEFS                                                                        \
  __asm  (                                                                          \
  "cgetpcc    $c17              \n"                                                 \
  "cincoffset $c17, $c17, (3*4) \n"                                                 \
    CCALL_SLOTLESS                                                                  \
    : LW_ASM(OutT)                                                                  \
    : "C" (_code_tmp), "C"(_data_tmp), "C"(_data_link) EXTRA_IN                     \
    : LW_CLOB(OutT), "a1", "a2", "a3", "$c1", "$c2", "$c4", "$c5", "$c6", "$c17", "memory" \
  );                                                                                \
  LW_VAl(OutT)                                                                      \
})

#endif //CHERIOS_LIGHTWEIGHT_CCALL_H
