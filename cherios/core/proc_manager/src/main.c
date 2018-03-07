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
#include "act_events.h"
#include "capmalloc.h"
#include "tman.h"

/* See init.S in libuser for conventions */

process_t loaded_processes[MAX_PROCS];
size_t processes_end = 0;
capability sealer;

Elf_Env env;

int bootstrapping = 1;

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

process_t* seal_proc_for_user(process_t* process) {
	return cheri_seal(process, sealer);
}

process_t* unseal_proc(process_t* process) {
	if(cheri_gettype(process) != cheri_getcursor(sealer)) return NULL;
	return cheri_unseal(process, sealer);
}

static process_t* unseal_live_proc(process_t* process) {
	process = unseal_proc(process);

	if(process == NULL) return process;

	enum process_state state;

	ENUM_VMEM_SAFE_DEREFERENCE(&process->state, state, proc_zombie);

	if(state == proc_zombie) return NULL;

	return process;
}

/* Trampoline used as entry point for all secure loaded programs. Simply calls nano kernel enter. */
__asm__ (
	SANE_ASM
	".text\n"
	".global secure_entry_trampoline\n"
	"secure_entry_trampoline: ccall $c1, $c2, 2\n"
	"nop\n"
);
extern void secure_entry_trampoline(void);

static mop_t make_mop_for_process(void) {
	if(bootstrapping) return NULL;
	res_t  space = cap_malloc(MOP_REQUIRED_SPACE);
	assert(space != NULL);
	mop_t mop = mem_makemop(space, own_mop).val;
	assert(mop != NULL);
	return mop;
}

static act_control_kt create_activation_for_image(image* im, const char* name, register_t arg, capability carg, capability pcc,
                                                  char* stack_args, size_t stack_args_size, process_t * process, uint8_t cpu_hint) {
    reg_frame_t frame;
    memset(&frame, 0, sizeof(reg_frame_t));

    env.handle = process->mop;

    queue_t* queue = setup_c_program(&env, &frame, im, arg, carg, pcc, stack_args, stack_args_size, process->mop);

	frame.mf_t0 = cheri_getbase(im->seg_table[im->code_index]);

    if(queue == NULL) {
        cap_pair pair = env.alloc(0x100, &env);
        CHERI_PRINT_CAP(pair.data);
    }
	assert(queue != NULL);

    frame.cf_c22 = seal_proc_for_user(process);

	if(process->im.secure_loaded) {
		frame.cf_pcc = &secure_entry_trampoline;
		// we need c3 for the trampoline. C0 would be useless anyway as it points to the unsecure copy
		// frame.cf_c0 = frame.cf_c3;
		// frame.cf_c1 = STUB_STRUCT_RO(foundation_enter)->c1;
		// frame.cf_c2 = PLT_UNIQUE_OBJECT(nano_kernel_if_t);
		// frame.cf_c3 = process->im.secure_entry;
	}

	act_control_kt ctrl = syscall_act_register(&frame, name, queue, cap_malloc(ACT_REQUIRED_SPACE), cpu_hint);

	act_kt act = syscall_act_ctrl_get_ref(ctrl);

    if(event_act == NULL) try_set_event_source();
	if(event_act != NULL) {
        int status = subscribe_terminate(act, act_self_ref, seal_proc_for_user(process), process->n_threads, 4);
        assert_int_ex(status, ==, SUBSCRIBE_OK);
    }

	return ctrl;
}

act_control_kt create_thread(process_t * process, const char* name, register_t arg, capability carg, capability pcc,
							 char* stack_args, size_t stack_args_size, uint8_t cpu_hint) {

	if(process->state != proc_started) return NULL;

	assert(process->n_threads != MAX_THREADS);

	env.handle = process->mop;

	create_image(&env, &process->im, &process->im, storage_thread);

	act_control_kt ctrl = create_activation_for_image(&process->im, name, arg, carg, pcc,
													  stack_args, stack_args_size, process, cpu_hint);

	process->threads[process->n_threads++] = ctrl;
	return ctrl;
}

process_t* create_process(const char* name, capability file, int secure_load) {

	process_t* proc;

	assert(secure_load == 0);

	/* We can save some work by just copying an already loaded image. Later on we will allow some sharing, e.g. of
    * const data / text */
	image* old_im = find_process(name);

	proc = alloc_process(name);

	proc->state = proc_created;
	proc->n_threads = 0;
	proc->terminated_threads = 0;
	proc->mop = make_mop_for_process();

	env.handle = proc->mop;

	if(old_im == NULL) {
		elf_loader_mem(&env, (Elf64_Ehdr*)file, &proc->im);
	} else {
		create_image(&env, old_im, &proc->im, storage_process);
	}

	return seal_proc_for_user(proc);
}

act_control_kt start_process(process_t* proc, register_t arg, capability carg, char* stack_args, size_t stack_args_size, uint8_t cpu_hint) {
	assert(proc->n_threads == 0);
	assert(proc->state == proc_created);

	proc->state = proc_started;

	void * pcc = make_global_pcc(&proc->im);

	act_control_kt ctrl = create_activation_for_image(&proc->im, proc->name, arg, carg, pcc,
													  stack_args, stack_args_size, proc, cpu_hint);

	proc->threads[proc->n_threads++] = ctrl;
	return ctrl;
}

act_control_kt user_start_process(process_t* proc, startup_desc_t* desc) {
    proc = unseal_proc(proc);
    return start_process(proc, desc->arg, desc->carg, desc->stack_args, desc->stack_args_size, desc->cpu_hint);
}

act_control_kt user_create_thread(process_t* proc, const char* name, startup_desc_t* desc) {
    proc = unseal_proc(proc);
    act_control_kt* ctrl = create_thread(proc, name, desc->arg, desc->carg,
                                         desc->pcc, desc->stack_args, desc->stack_args_size, desc->cpu_hint);
    return ctrl;
}

static void deliver_mop(mop_t mop) {
	/* Normally a process would already have these, but we were created before memmgt */
	if(own_mop == NULL) {
		mmap_set_mop(mop);
		try_init_memmgt_ref();
		bootstrapping = 0;
	}
}

static top_t proc_get_own_top(void) {
	// For now proc_man will grab the first top.
	static top_t own_top;

	if(own_top == NULL) {
		assert(try_init_tman_ref() != NULL);
		own_top = type_get_first_top();
		assert(own_top != NULL);
	}
	return own_top;
}

static top_t __get_top_for_process(process_t* proc) {

	proc = unseal_live_proc(proc);

	if(proc == NULL) return NULL;

	if(proc->top == NULL) {
		top_t own_top = proc_get_own_top();
		ERROR_T(top_t) new = type_new_top(own_top);
		if(!IS_VALID(new)) return NULL;
		proc->top = new.val;
	}

	return proc->top;
}

static void handle_termination(register_t thread_num, process_t* proc, act_kt target) {

    proc = unseal_proc(proc);

	assert(proc->state == proc_started);
	assert(proc->threads[thread_num] != NULL);

	printf("Process %s terminated thread %lx\n", proc->name, thread_num);

	proc->threads[thread_num] = NULL;
	proc->terminated_threads++;

	if(proc->terminated_threads == proc->n_threads) {
		printf("Last thread terminated, killing process\n");
		proc->state = proc_zombie;
		int status = mem_reclaim_mop(proc->mop);
		assert_int_ex(status, ==, MEM_OK);
		if(proc->top) {
			er_t er = type_destroy_top(proc->top);
			assert_int_ex((int64_t)er, ==, TYPE_OK);
		}
	}
}


void (*msg_methods[]) = {create_process, user_start_process, user_create_thread, deliver_mop, handle_termination, __get_top_for_process};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);


cap_pair proc_tmp_alloc(size_t s, Elf_Env* the_env) {
    /* Swaps to using the proper alloc when memmgt is up */
    if(namespace_get_ref(namespace_num_memmgt) != NULL) {
        the_env->alloc = &mmap_based_alloc;
        the_env->free = &mmap_based_free;
        return mmap_based_alloc(s, the_env);
    } else {
        return tmp_alloc(s, the_env);
    }
}

int main(procman_init_t* init)
{

    init_tmp_alloc(init->pool_from_init);

    env.alloc = &proc_tmp_alloc;
    env.free = &tmp_free;
    env.printf = &printf;
    env.memcpy = &memcpy;
    env.vprintf = &vprintf;

	sealer = init->sealer;

	namespace_register(namespace_num_proc_manager, act_self_ref);
    msg_enable = 1;
}
