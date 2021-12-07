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
#include "stdlib.h"

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

// TODO now that we have a memory system, we should require the caller to allocate this struct.
// TODO leaving it as use once for now
static process_t* alloc_process(const char* name) {
	assert(processes_end != MAX_PROCS);
	loaded_processes[processes_end].name = name;
	return cheri_setbounds_exact(&loaded_processes[processes_end++], sizeof(process_t));
}

process_t* seal_proc_for_user(process_t* process) {
	return cheri_seal(process, sealer);
}

uint8_t alloc_thread_n(process_t* process) {
	int8_t hd = process->free_hd;
	uint8_t res;
	if(hd >= 0) {
		res = (uint8_t)hd;
		hd++;
	} else {
		hd = ~hd;
		res = (uint8_t)hd;
		hd = *(int8_t*)(&process->threads[res]);
	}
	process->free_hd = hd;
	return res;
}

void free_thread_n(process_t* process, uint8_t threadn) {
	*(int8_t*)(&process->threads[threadn]) = process->free_hd;
	process->free_hd = ~threadn;
}

process_t* unseal_proc(process_t* process) {
    return cheri_unseal_2(process, sealer);
}

static process_t* unseal_live_proc(process_t* process) {
	process = unseal_proc(process);

	if(process == NULL) return process;

	enum process_state state;

	ENUM_VMEM_SAFE_DEREFERENCE(&process->state, state, proc_zombie);

	if(state == proc_zombie) return NULL;

	return process;
}

extern void secure_entry_trampoline(void);

static mop_t make_mop_for_process(process_t* proc) {
	if(bootstrapping) return NULL;
	res_t  space = cap_malloc(MOP_REQUIRED_SPACE);
	assert(space != NULL);
	ERROR_T(mop_t) mop_or_er = mem_makemop_debug(space, own_mop, proc->name);
	mop_t mop = mop_or_er.val;
	if(!IS_VALID(mop_or_er)) {
		assert_int_ex(-mop_or_er.er, ==, 0);
	}
	assert(mop != NULL);
	return mop;
}

static top_t proc_get_own_top(void) {
	// For now proc_man will grab the first top.
	static top_t own_top;

	if(own_top == NULL) {
		__unused act_kt tman = try_init_tman_ref();
		assert(tman != NULL);
		own_top = type_get_first_top();
		assert(own_top != NULL);
	}
	return own_top;
}

static top_t top_for_process(process_t* proc) {
    if(proc->top == NULL) {
        top_t own_top = proc_get_own_top();
        ERROR_T(top_t) new = type_new_top(own_top);
        if(!IS_VALID(new)) return NULL;
        proc->top = new.val;
    }

    return proc->top;
}

// To defeat bootstrapping problems we allow the process manager to allocate some type reservations
#define TMP_TRES_START 0x800
#define TMP_TRES_STOP  0x900

static sealing_cap get_tres_for_process(process_t* process) {
	static uint64_t type = TMP_TRES_START;

	if(type != TMP_TRES_STOP) {
		if(try_init_tman_ref() != NULL) type = TMP_TRES_STOP;
		else return tres_get(type++);
	}

	return type_get_new(top_for_process(process)).val;
}

static act_control_kt create_activation_for_image(image* im, const char* name, register_t arg, capability carg, capability pcc,
                                                  char* stack_args, size_t stack_args_size, process_t * process,
                                                  uint8_t cpu_hint, startup_flags_e flags, cert_t found_cert) {
    reg_frame_t frame;
    memset(&frame, 0, sizeof(reg_frame_t));

    env.handle = process->mop;

    queue_t* queue = setup_c_program(&env, &frame, im, arg, carg, pcc, stack_args, stack_args_size, process->mop);

    uint64_t base;
    if(im->tls_num == 1) {
        if(process->im.secure_loaded) {
           base = foundation_entry_vaddr(process->im.load_type.secure.secure_entry) - process->im.entry;
        } else {
            base = cheri_getbase(im->load_type.basic.tables.seg_table[im->code_index]);
        }
        process->load_base = base;
    } else base = process->load_base;

    frame.cf_program_base = base;

    if(queue == NULL) {
        cap_pair pair = env.alloc(0x100, &env);
        CHERI_PRINT_CAP(pair.data);
    }
	assert(queue != NULL);

    frame.cf_proc_ref = seal_proc_for_user(process);

	if(process->im.secure_loaded) {
		frame.cf_pcc = &secure_entry_trampoline;
		// we need c3 for the trampoline. C0 would be useless anyway as it points to the unsecure copy
  		frame.cf_found_enter = &foundation_enter_dummy;
	 	frame.cf_nano_if_data = &PLT_UNIQUE_OBJECT(nano_kernel_if_t);
	 	frame.cf_type_res = get_tres_for_process(process);
		frame.cf_cert = found_cert;
	}

	frame.cf_start_flags = flags;

	act_control_kt ctrl = syscall_act_register(&frame, name, queue, bootstrapping ? NULL : cap_malloc_need_split(ACT_REQUIRED_SPACE), cpu_hint);

	act_kt act = syscall_act_ctrl_get_ref(ctrl);

	process->n_threads++;

	uint8_t thread_n = alloc_thread_n(process);

	assert(thread_n != MAX_THREADS_PER_PROC);

    if(event_act == NULL) try_set_event_source();
	if(event_act != NULL) {
        __unused int status = subscribe_terminate(act, act_self_ref, seal_proc_for_user(process), thread_n, 4);
        assert_int_ex(status, ==, SUBSCRIBE_OK);
    }

	process->threads[thread_n] = ctrl;

	return ctrl;
}

act_control_kt create_thread(process_t * process, const char* name, register_t arg, capability carg, capability pcc,
							 char* stack_args, size_t stack_args_size, uint8_t cpu_hint, startup_flags_e flags,
							 cert_t found_cert) {

	if(process->state != proc_started) return NULL;

	assert(process->n_threads != MAX_THREADS_PER_PROC);

	env.handle = process->mop;

	create_image(&env, &process->im, &process->im, storage_thread);

	act_control_kt ctrl = create_activation_for_image(&process->im, name, arg, carg, pcc,
													  stack_args, stack_args_size, process, cpu_hint, flags, found_cert);

	return ctrl;
}

process_t* create_process(const char* name, capability file, int secure_load) {

	process_t* proc;

	/* We can save some work by just copying an already loaded image. Later on we will allow some sharing, e.g. of
    * const data / text */
	image* old_im = find_process(name);

	proc = alloc_process(name);

	proc->state = proc_created;
	proc->mop = make_mop_for_process(proc);

	env.handle = proc->mop;

	if(old_im == NULL) {
		elf_loader_mem(&env, (Elf64_Ehdr*)file, &proc->im, secure_load);
	} else {
	    assert(secure_load == old_im->secure_loaded);
		create_image(&env, old_im, &proc->im, storage_process);
	}

	return seal_proc_for_user(proc);
}

act_control_kt start_process(process_t* proc, register_t arg, capability carg, char* stack_args, size_t stack_args_size,
							 uint8_t cpu_hint, startup_flags_e flags, cert_t found_cert) {
	assert(proc->n_threads == 0);
	assert(proc->state == proc_created);

	proc->state = proc_started;

	void * pcc = make_global_pcc(&proc->im);

	act_control_kt ctrl = create_activation_for_image(&proc->im, proc->name, arg, carg, pcc,
													  stack_args, stack_args_size, proc, cpu_hint, flags, found_cert);

	return ctrl;
}

act_control_kt user_start_process(process_t* proc, startup_desc_t* desc) {
    proc = unseal_proc(proc);
    return start_process(proc, desc->arg, desc->carg, desc->stack_args, desc->stack_args_size,
    		desc->cpu_hint, desc->flags, desc->inv);
}

act_control_kt user_create_thread(process_t* proc, const char* name, startup_desc_t* desc) {
    proc = unseal_proc(proc);
    act_control_kt ctrl = create_thread(proc, name, desc->arg, desc->carg,
                                         desc->pcc, desc->stack_args, desc->stack_args_size, desc->cpu_hint,
                                         desc->flags, desc->inv);
    return ctrl;
}

static void deliver_mop(mop_t mop) {
	/* Normally a process would already have these, but we were created before memmgt */
	if(own_mop == NULL) {
		assert(cheri_gettag(mop));
		mmap_set_mop(mop);
		try_init_memmgt_ref();
		bootstrapping = 0;
	}
}

static top_t __get_top_for_process(process_t* proc) {

	proc = unseal_live_proc(proc);

	if(proc == NULL) return NULL;

    return top_for_process(proc);
}

static void handle_termination(register_t thread_num, process_t* proc, __unused act_kt target) {

    proc = unseal_proc(proc);

	assert(proc->state == proc_started);
	assert(cheri_gettag(proc->threads[thread_num]));

	printf("Process %s terminated thread %lx\n", proc->name, thread_num);

	free_thread_n(proc, thread_num);

	proc->terminated_threads++;
	proc->n_threads--;

	if(proc->n_threads == 0) {
		printf("Last thread terminated, killing process\n");
		proc->state = proc_zombie;
		__unused int status = mem_reclaim_mop(proc->mop);
		assert_int_ex(status, ==, MEM_OK);
		if(proc->top) {
			__unused er_t er = type_destroy_top(proc->top);
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
    if(!bootstrapping && namespace_get_ref(namespace_num_memmgt) != NULL) {
        the_env->alloc = &mmap_based_alloc;
        the_env->free = (typeof(the_env->free))&mmap_based_free;
        return mmap_based_alloc(s, the_env);
    } else {
        return tmp_alloc(s, the_env);
    }
}

int main(procman_init_t* init)
{

	// This handles a race between starting procman and init being finished with the tmp alloc pool
	while(init->pool_from_init.code == NULL || init->pool_from_init.data == NULL) {
		sleep(0);
	}

    init_tmp_alloc(init->pool_from_init);

    env.alloc = &proc_tmp_alloc;
    env.free = &tmp_free;
    env.printf = &printf;
    env.memcpy = &memcpy;
    env.vprintf = &vprintf;

	sealer = init->sealer;

	namespace_register(namespace_num_proc_manager, act_self_ref);
    msg_enable = 1;

    return 0;
}
