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

#include "object.h"
#include "misc.h"
#include "proc.h"
#include "elf.h"
#include "assert.h"
#include "string.h"
#include "cprogram.h"
#include "stdio.h"
#include "thread.h"
#include "namespace.h"
#include "tmpalloc.h"
#include "../../boot/include/boot/boot_info.h"

ALLOCATE_PLT_NANO

/*Some "documentation" for the interface between the kernel and activation start                                        *
* These fields are setup by the caller of act_register                                                                  *
*                                                                                                                       *
* a0    : user GP argument (goes to main)                                                                               *
* c3    : user Cap argument (goes to main)                                                                              * *
*                                                                                                                       *
* These fields are setup by act_register itself. Although the queue is an argument to the function                      *
*                                                                                                                       *
* c21   : self control reference
* c22   : process reference                                                                                             *
* c23   : namespace reference (may be null for init and namespace)                                                      *
* c24   : kernel interface table                                                                                        *
* c25   : queue                                                                                                        */


process_t loaded_processes[MAX_PROCS];
size_t processes_end = 0;

Elf_Env env;

static image* find_process(const char *name) {
	for(size_t i = 0; i < processes_end; i++) {
		if(strcmp(name, loaded_processes[i].name) == 0) return &(loaded_processes[i].im);
	}
	return NULL;
}

static process_t* alloc_process(const char* name) {
	assert(processes_end != MAX_PROCS);
	loaded_processes[processes_end].name = name;
	return &loaded_processes[processes_end++];
}

// TODO
process_t* seal_proc_for_user(process_t* process) {
	return process;
}

// TODO
process_t* unseal_proc(process_t* process) {
	return process;
}

/* Trampoline used as entry point for all secure loaded programs. Simply calls nano kernel enter. */
__asm__ (
	".text\n"
	".global secure_entry_trampoline\n"
	"secure_entry_trampoline: ccall $c1, $c2\n"
);
extern void secure_entry_trampoline(void);

static act_control_kt create_activation_for_image(image* im, const char* name, register_t arg, capability carg, capability pcc,
                                                  char* stack_args, size_t stack_args_size, process_t * process) {
    reg_frame_t frame;
    memset(&frame, 0, sizeof(reg_frame_t));

    queue_t* queue = setup_c_program(&env, &frame, im, arg, carg, pcc, stack_args, stack_args_size);

    frame.cf_c22 = seal_proc_for_user(process);

	if(process->im.secure_loaded) {
		frame.cf_pcc = &secure_entry_trampoline;
		// we need c3 for the trampoline. C0 would be useless anyway as it points to the unsecure copy
		frame.cf_c0 = frame.cf_c3;
		frame.cf_c1 = foundation_enter_default_obj.code;
		frame.cf_c2 = foundation_enter_default_obj.data;
		frame.cf_c3 = process->im.secure_entry;
	}

    return syscall_act_register(&frame, name, queue, get_res_pool());
}

act_control_kt create_thread(process_t * process, const char* name, register_t arg, capability carg, capability pcc,
							 char* stack_args, size_t stack_args_size) {

	assert(process->n_threads != MAX_THREADS);

    reg_frame_t frame;

	create_image(&env, &process->im, &process->im, storage_thread);

	act_control_kt ctrl = create_activation_for_image(&process->im, name, arg, carg, pcc,
													  stack_args, stack_args_size, process);

	process->threads[process->n_threads++] = ctrl;
	return ctrl;
}

process_t* create_process(const char* name, capability file, int secure_load) {

	process_t* proc;

	/* We can save some work by just copying an already loaded image. Later on we will allow some sharing, e.g. of
    * const data / text */
	image* old_im = find_process(name);

	proc = alloc_process(name);

	cap_pair prgmp;

	if(old_im == NULL) {
		prgmp = elf_loader_mem(&env, file, &proc->im, secure_load);
	} else {
		prgmp = create_image(&env, old_im, &proc->im, storage_process);
	}

	if(!prgmp.data) {
		assert(0);
		return NULL;
	}

	proc->n_threads = 0;

	return seal_proc_for_user(proc);
}

act_control_kt start_process(process_t* proc, register_t arg, capability carg, char* stack_args, size_t stack_args_size) {
	assert(proc->n_threads == 0);

	cap_pair prgmp = proc->im.loaded_process;

	void * pcc = cheri_setoffset(prgmp.code, (proc->im).entry);
	pcc = cheri_andperm(pcc, (CHERI_PERM_GLOBAL | CHERI_PERM_EXECUTE | CHERI_PERM_LOAD
							  | CHERI_PERM_LOAD_CAP));

	act_control_kt ctrl = create_activation_for_image(&proc->im, proc->name, arg, carg, pcc,
													  stack_args, stack_args_size, proc);

	proc->threads[proc->n_threads++] = ctrl;
	return ctrl;
}

act_control_kt user_start_process(process_t* proc, startup_desc_t* desc) {
    proc = unseal_proc(proc);
    return start_process(proc, desc->arg, desc->carg, desc->stack_args, desc->stack_args_size);
}

act_control_kt user_create_thread(process_t* proc, const char* name, startup_desc_t* desc) {
    proc = unseal_proc(proc);
    act_control_kt* ctrl = create_thread(proc, name, desc->arg, NULL, desc->pcc, desc->stack_args, desc->stack_args_size);
    return ctrl;
}

void (*msg_methods[]) = {create_process, user_start_process, user_create_thread};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

cap_pair proc_tmp_alloc(size_t s) {
    /* Swaps to using the proper alloc when memmgt is up */
    if(namespace_get_ref(namespace_num_memmgt) != NULL) {
        env.alloc = &mmap_based_alloc;
        env.free = &mmap_based_free;
        return mmap_based_alloc(s);
    } else {
        return tmp_alloc(s);
    }
}

int main(procman_init_t* init)
{
	init_nano_kernel_if_t(init->nano_if, init->nano_default_cap);

    init_tmp_alloc(init->pool_from_init);

    env.alloc = &proc_tmp_alloc;
    env.free = &tmp_free;
    env.printf = &printf;
    env.memcpy = &memcpy;
    env.vprintf = &vprintf;
	env.mmap_new = mmap_new;

	namespace_register(namespace_num_proc_manager, act_self_ref);
    msg_enable = 1;
}
