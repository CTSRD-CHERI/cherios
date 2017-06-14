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

#include "cprogram.h"
#include "queue.h"
#include "string.h"
#include "syscalls.h"

act_control_kt simple_start(Elf_Env* env, const char* name, capability file, register_t arg, capability carg, act_kt namespace) {
    image im;
    reg_frame_t frame;
    bzero(&frame, sizeof(frame));

    cap_pair prgmp = elf_loader_mem(env, file, &im);

    void * pcc = cheri_setoffset(prgmp.code, im.entry);
    pcc = cheri_andperm(pcc, (CHERI_PERM_GLOBAL | CHERI_PERM_EXECUTE | CHERI_PERM_LOAD
                              | CHERI_PERM_LOAD_CAP));

    queue_t* queue = setup_c_program(env, &frame, &im, arg, carg, pcc , NULL, 0, namespace);

    return syscall_act_register(&frame, name, queue, NULL);
}

queue_t* setup_c_program(Elf_Env* env, reg_frame_t* frame, image* im, register_t arg, capability carg,
			 capability pcc, char* stack_args, size_t stack_args_size, act_kt namespace) {
    cap_pair prgmp = im->loaded_process;

    size_t low_bits = cheri_getbase(prgmp.data) & (0x20 - 1);

    if(low_bits != 0) {
        env->printf("ERROR: alignment of loaded file was %ld\n", low_bits);
        return NULL;
    }

    size_t stack_size = 0x10000;
    size_t stack_align = 0x40;
    size_t queue_size = ((sizeof(queue_default_t) + stack_align - 1) / stack_align) * stack_align;
    void * stack = env->alloc(stack_size).data;
    if(!stack) {
        return NULL;
    }

    /* Steal a few bytes from the bottom of the stack to use as the message queue */
    queue_t* queue = (queue_t*)((char*)stack + stack_size - queue_size);

    queue = cheri_setbounds(queue, queue_size);
    stack = cheri_setbounds(stack, stack_size - queue_size);

    /* allows us to pass stack arguments to a new activation */
    if(stack_args_size != 0) {
        env->memcpy(stack + cheri_getlen(stack) - stack_args_size, stack_args, stack_args_size);
    }

    /* set pc */
    frame->cf_pcc	= pcc;

    /* Setup user local reg */

    frame->mf_user_loc = 0x7000 +
                        (im->tls_base + (im->tls_size * (im->tls_num-1)));

    /* set stack */
    frame->cf_c11	= stack;
    frame->mf_sp	= (cheri_getlen(stack) - stack_args_size);

    /* set c12 */
    frame->cf_c12	= frame->cf_pcc;

    /* set c0 */
    frame->cf_c0	= prgmp.data;

    /* set namespace */
    frame->cf_c23       = namespace;

    /* Setup args */
    frame->mf_a0 = arg;
    frame->cf_c3 = carg;

    return queue;
}
