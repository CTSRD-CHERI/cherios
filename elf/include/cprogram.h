/*-
 * Copyright (c) 2017 Lawrence Esswood
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

#ifndef CHERIOS_CPROGRAM_H
#define CHERIOS_CPROGRAM_H

// We request our default queue (about 0x820 ) and the queue in one go. We can do exact bounds on 0xff00.
// Sadly requesting via alloc with mmap can only give us 0xfe00.
// so a stack of f600 + 0x900 for the queue works nicely and wastes about 0x1c0 bytes. A bit of work could make this 0xc0.

#define DEFAULT_STACK_SIZE_NO_QUEUE 0xff80

#define DEFAULT_STACK_SIZE 0xf600
#define DEFAULT_STACK_ALIGN_p2 8
#define DEFAULT_STACK_ALIGN (1 << DEFAULT_STACK_ALIGN_p2)


#ifndef __ASSEMBLY__

#include "cheric.h"
#include "elf.h"
#include "queue.h"
#include "mman.h"

act_control_kt simple_start(Elf_Env* env, const char* name, const char* file, register_t arg, capability carg, mop_t mop, image* im);

queue_t* setup_c_program(Elf_Env* env, reg_frame_t* frame, image* im, register_t arg, capability carg,
                     capability pcc, char* stack_args, size_t stack_args_size, mop_t mop);

#endif

#endif //CHERIOS_CPROGRAM_H
