/*-
 * Copyright (c) 2020 Lawrence Esswood
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

#define TRACE_UNALIGNED 0

#if (TRACE_UNALIGNED)
    #ifndef  LIGHTWEIGHT_OBJECT
        #include "stdio.h"
        #define TRC(...) printf(__VA_ARGS__)
        #define TRC_CAP(X) CHERI_PRINT_CAP(X)
    #endif
#endif

#ifndef  TRC
    #define TRC(...)
    #define TRC_CAP(X)
#endif


#include "exceptions.h"
#include "cheric.h"
#include "mips.h"

// Currently handles CLX/CSX where X one of B,H,W,D,BU,HU,WU,D

// Can only handle in delay slots of CJR/CJALR/BEQ/BopZ


#define OPCODE_SHIFT    26
#define CSX_CODE        0x3a

#define CLX_CODE        0x32

#define CJX_CODE        0x12

#define BEQ_CODE        0x4

#define BNE_CODE        0x5

#define BGEZ_CODE       0x1
#define BGEZ_OP         1

#define BLTZ_CODE       0x1
#define BLTZ_OP         0

#define BLEZ_CODE       0x6
#define BLEZ_OP         0

#define BGTZ_CODE       0x7
#define BGTZ_OP         0





#define CJR_BOT         0x07ff
#define CJALR_BOT       0x033f
#define CJX_BOT_MASK    0x07ff

#define T_MASK 0b11 // immediate scale
#define S_MASK 0b100 // signed

// Immediate offset (scaled by 2^T)
#define OFFSET_SHIFT 3
#define OFFSET_MASK 0xFF
#define OFFSET_TOP_BIT 0x80

// Register offset (unsigned)
#define RT_SHIFT 11
#define RT_MASK 0b11111

// Capability base
#define CB_SHIFT 16
#define CB_MASK 0b11111

// Destination Register
#define RD_SHIFT 21
#define RD_MASK 0b11111

register_t* get_r_reg(exception_restore_frame* restore_frame, exception_restore_saves_frame* saves_frame, uint32_t index) {

    if(index == 0 || index == 26 || index == 27) return NULL;

    // Normal contains first 1 to 15, then 24 to 25, then 28 to 31
    // Saves contains 16 to 23

    if(index <= 15) {
        return (&restore_frame->mf_at) + (index - 1);
    } else if(index <= 23) {
        return (&saves_frame->mf_s0) + (index - 16);
    } else if(index <= 25) {
        return (&restore_frame->mf_t8) + (index - 24);
    } else {
        return (&restore_frame->mf_gp) + (index - 28);
    }

}

capability* get_c_reg(exception_restore_frame* restore_frame, exception_restore_saves_frame* saves_frame, uint32_t index) {

    if(index == 0 || index > 26) return NULL;

    if(index == 10) {
#ifdef USE_EXCEPTION_UNSAFE_STACK
        return restore_frame->c10;
#else
        return NULL;
#endif
    }

    if(index == 11) {
#ifdef USE_EXCEPTION_STACK
        return restore_frame->c11;
#else
        return NULL;
#endif
    }

    if(index == 1) return &get_ctl()->ex_c1;
    if(index == 25) return &restore_frame->c25;
    if(index == 26) return &get_ctl()->ex_idc;

    // Normal frame contains c2 to c9, then c12 to c18, then c25
    // Saves frame contains c19 to c24

    if(index <= 9) {
        return (&restore_frame->c2) + (index - 2);
    } else if(index <= 18) {
        return (&restore_frame->c12) + (index - 12);
    } else {
        return (&saves_frame->c19) + (index - 19);
    }

}

#define GET_C(N) ({capability* _c_tmp = get_c_reg(restore_frame, saves_frame, N); _c_tmp ? *_c_tmp : NULL;})
#define GET_R(N) ({register_t* _r_tmp = get_r_reg(restore_frame, saves_frame, N); _r_tmp ? *_r_tmp : 0;})
#define SET_R(N, V) ({register_t* _r_tmp = get_r_reg(restore_frame, saves_frame, N); if(_r_tmp) *_r_tmp = V;})
#define SET_C(N, V) ({capability* _c_tmp = get_c_reg(restore_frame, saves_frame, N); if(_c_tmp) *_c_tmp = V;})


int emulate_skip(exception_restore_frame* restore_frame, exception_restore_saves_frame* saves_frame, int is_delay_slot) {
    uint32_t* ex_pcc = (uint32_t*)get_ctl()->ex_pcc;

    uint32_t* next = ex_pcc+1;

    if(is_delay_slot) {
        TRC("Emulate slot\n");
        // GRRRRRRRR delay slots
        uint32_t instr = *ex_pcc;

        uint32_t op = instr >> OPCODE_SHIFT;

        if(op == CJX_CODE) {
            uint32_t bot = op & CJX_BOT_MASK;
            capability* arg1_ptr =  get_c_reg(restore_frame, saves_frame, (instr >> 16) & 0b11111);

            if(bot == CJR_BOT) {
                // emulate CJR
                next = (uint32_t*)(*arg1_ptr);
            } else if(bot == CJALR_BOT) {
                // emulate CJALR
                if(arg1_ptr) *arg1_ptr = next+1;
                capability arg2 = GET_C((instr >> 11) & 0b11111);
                next = (uint32_t*)arg2;
            } else return -1;
        } else {
            // Branch instructions

            // Offset
            int64_t offset = (instr & 0xFFFF);
            // Sign extend
            offset = offset | -(instr & 0x8000);

            int should_branch;

            register_t rs = GET_R((instr >> 21) & 0b11111);

            uint32_t RT_V = (instr >> 16) & 0b11111;

            register_t rt = GET_R(RT_V);

            if(op == BEQ_CODE) {
                should_branch = rs == rt;
            } else if(op == BNE_CODE) {
                should_branch = rs != rt;
            } else if(op == BLTZ_CODE && RT_V == BLTZ_OP) {
                should_branch = rs < 0;
            } else if(op == BLEZ_CODE && RT_V == BLEZ_OP) {
                should_branch = rs <= 0;
            } else if(op == BGEZ_CODE && RT_V == BGEZ_OP) {
                should_branch = rs >= 0;
            } else if(op == BGTZ_CODE && RT_V == BGTZ_OP) {
                should_branch = rs > 0;
            } else return -1;

            next = should_branch ? next + offset : next + 1;
        }
    }

    get_ctl()->ex_pcc = (capability)next;

    return 0;
}

int handle_unaligned(register_t cause, __unused register_t ccause, exception_restore_frame* restore_frame, exception_restore_saves_frame* saves_frame) {

    TRC("Emulate unaligned\n");

    uint32_t* ex_pcc = (uint32_t*)get_ctl()->ex_pcc;
    uint32_t* ex_instr = ex_pcc;

    if(cause & MIPS_CP0_CAUSE_BD) ex_instr++;

    uint32_t instr = *ex_instr;

    uint32_t t = instr & T_MASK;
    uint32_t s = instr & S_MASK;

#define GET_FIELD(F) ((instr >> F ## _SHIFT) & (F ## _MASK))

    uint64_t offset_unextended = (uint64_t)GET_FIELD(OFFSET);

    // Properly sign extend
    int64_t offset = (int64_t)offset_unextended | ~((offset_unextended & 0x80) - 1);
    // Scale by 2^t
    offset <<= t;

    uint32_t rt = GET_FIELD(RT);
    uint32_t cb = GET_FIELD(CB);
    uint32_t rd = GET_FIELD(RD);

    char* ptr = (char*)GET_C(cb);
    ptr += offset;
    ptr += GET_R(rt);

    uint32_t nbytes = 1 << t;

    if((instr >> OPCODE_SHIFT) == CSX_CODE) {
        // Store byte by byte (backwards is easier because going from the little end is easier)
        register_t val = GET_R(rd);

        TRC("Store %lx to ", val);
        TRC_CAP(ptr);
        TRC(" width = %d. sing %d\n", nbytes, s);

        uint8_t* dst = (uint8_t*)(ptr + nbytes - 1);

        do {
            *dst = (uint8_t)(val);
            val >>= 8;
        } while(dst-- != (uint8_t*)ptr);

    } else if((instr >> OPCODE_SHIFT) == CLX_CODE) {
        // Load byte by byte (first byte special due to sign-ed-ness)

        TRC("Loaded from ");
        TRC_CAP(ptr);
        TRC(" width = %d. sing %d\n", nbytes, s);

        register_t val = (register_t)(*(int8_t*)ptr);
        if(!s) val &= 0xFF;

        uint8_t* src = (uint8_t*)ptr + 1;
        nbytes --;

        do {
            val = (val << 8) | *src;
            src ++;
            nbytes --;
        } while(nbytes != 0);

        TRC("Result %lx\n", val);

        SET_R(rd, val);

    } else return -1;

    return emulate_skip(restore_frame, saves_frame, cause & MIPS_CP0_CAUSE_BD);
}