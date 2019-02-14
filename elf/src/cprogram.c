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

#include <elf.h>
#include "cprogram.h"
#include "queue.h"
#include "string.h"
#include "syscalls.h"
#include "mman.h"
#include "assert.h"

act_control_kt simple_start(Elf_Env* env, const char* name, capability file, register_t arg, capability carg, mop_t mop, image* im) {
    reg_frame_t frame;
    bzero(&frame, sizeof(frame));

    int res = elf_loader_mem(env, (Elf64_Ehdr*)file, im, 0);

    void * pcc = make_global_pcc(im);

    queue_t* queue = setup_c_program(env, &frame, im, arg, carg, pcc , NULL, 0, mop);

    return syscall_act_register(&frame, name, queue, NULL, 0);
}

queue_t* setup_c_program(Elf_Env* env, reg_frame_t* frame, image* im, register_t arg, capability carg,
                     capability pcc, char* stack_args, size_t stack_args_size, mop_t mop) {

    size_t space_for_segs = im->secure_loaded ? 0 : sizeof(im->load_type.basic.seg_table);

    size_t queue_size = sizeof(queue_default_t);

    size_t stack_size = im->secure_loaded ?  0 : DEFAULT_STACK_SIZE;
    size_t stack_align = DEFAULT_STACK_ALIGN;

    // Request as much as we can without going over a page boundry
    size_t request_size = align_up_to(queue_size + stack_size + MEM_REQUEST_FAST_OFFSET, UNTRANSLATED_PAGE_SIZE) -
            MEM_REQUEST_FAST_OFFSET;

    void * space = env->alloc(request_size, env).data;
    if(!space) {
        return NULL;
    }

    /* Use the first few bytes for the queue */
    queue_t* queue = (queue_t*)(space);

    queue = cheri_setbounds_exact(queue, queue_size);

    /* now the stack */
    char* stack = space + queue_size;
    size_t extra_to_align = (size_t)(-(size_t)stack) & (DEFAULT_STACK_ALIGN-1);

    stack = cheri_setbounds_exact(stack + extra_to_align, stack_size);

    // Stack starts with space for a segment table and some stack passed args
    stack = (char*)stack + (stack_size - space_for_segs - stack_args_size);

    /* Use some more stack to pass the seg table */

    capability seg_tbl = stack + stack_args_size;

    /* allows us to pass stack arguments to a new activation */
    if(stack_args_size != 0) {
        assert(!im->secure_loaded); // Currently no stack args allowed for secure loaded things
        env->memcpy(stack, stack_args, stack_args_size);
    }

    /* set pc */
    frame->cf_pcc	= pcc;

    /* Setup thread local reg */
    frame->cf_idc = NULL;

    /* Setup the global reg */
    frame->cf_c25 = NULL;

    /* set stack */
    frame->cf_c11 = stack;
    /* TODO: Set unsafe stack here */

    /* set c12 */
    frame->cf_c12	= frame->cf_pcc;

    /* set default */
    frame->cf_default	= NULL;

    /* Setup args */
    frame->mf_a0 = arg;
    frame->cf_c3 = carg;

    frame->cf_c19 = mop;

    /* Set up a whole bunch of linking info */

    if(!im->secure_loaded) {
        env->memcpy(seg_tbl, im->load_type.basic.seg_table, sizeof(im->load_type.basic.seg_table));
        frame->cf_c4 = seg_tbl;                             // segment_table
        frame->cf_c5 = im->load_type.basic.tls_prototype ;                  // tls_prototype
        frame->cf_c6 = im->load_type.basic.code_write_cap;
        frame->mf_s1 = im->tls_index * sizeof(capability);  // tls_segment_offset
        frame->mf_a2 = im->data_index * sizeof(capability); // data_seg_offset
        frame->mf_a3 = im->data_vaddr;                      // data_seg_vaddr
        frame->mf_a4 = im->code_index * sizeof(capability); // code_seg_offset
        frame->mf_a5 = im->code_vaddr;                      // code_seg_vaddr
        frame->mf_a6 = im->tls_vaddr;                       // tls_seg_vaddr
        frame->mf_s3 = im->tls_fil_size;
        frame->mf_s5 = im->tls_mem_size;
    } else {
        frame->cf_c8 = im->load_type.secure.secure_entry;
    }

    frame->mf_t0 = cheri_getbase(pcc);
    return queue;
}