/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "mips.h"
#include "klib.h"
#include "lib.h"

typedef  struct
{
	int process_nb;
	void * object;
	void * stack;
	register_t stack_size;
}  sealed_data_t;

typedef  struct
{
	int proc;
	int ready;
	void * ctor;
	void * dtor;
	void ** methods;
	void * data_cap;
}  bind_t;
const int bindarrlen = 0x10;
bind_t bindarr[bindarrlen];

void kernel_object_init(void) {
	/* Zero the array of service->pid association */
	bzero(bindarr, sizeof(bindarr));
	bindarr[0].proc = 0; //kernel object
	for(int i=1; i<bindarrlen; i++) {
		bindarr[i].proc = -1;
	}
}

/* Create an object for service 'n' */
void kernel_object_get_object(int nb) {
	KERNEL_TRACE(__func__, "in");

	sealed_data_t *cb = NULL;

	if((bindarr[nb].proc < 0) || (!bindarr[nb].ready)) {
		goto end;
	}

	/* Create a task to anwser CCalls */
	cb = kernel_calloc(1, sizeof(sealed_data_t));
	cb->process_nb = task_create_bare();
	if(cb->process_nb < 0) {
		KERNEL_ERROR("Could not create task");
		goto end;
	}
	cb->stack = kernel_cp2_exception_framep[cb->process_nb].cf_c11;
	cb->stack_size = kernel_exception_framep[cb->process_nb].mf_sp; //TODO:hack
	
	kernel_cp2_exception_framep[cb->process_nb].cf_c0 = bindarr[nb].data_cap;
	
	cb->object = NULL;

	void * seal = __builtin_memcap_offset_set(__builtin_memcap_global_data_get(), nb);
	
	/* Call the constructor of the module */
	cb->object = ccall_n_c(bindarr[nb].ctor, __builtin_memcap_seal(cb, seal));
	
	/* Seal the object */
	cb = __builtin_memcap_seal(cb, seal);

	KERNEL_TRACE(__func__, "cb:%p", cb);

	end:
	creturn_c(cb);
}

/* Get methods of service 'n' */
void kernel_object_get_methods(int nb) {
	KERNEL_TRACE(__func__, "in");

	void * methods = NULL;

	if(bindarr[nb].proc < 0) {
		goto end;
	}

	methods = bindarr[nb].methods;

	end:
	creturn_c(methods);
}

static void (*kernel_methods[]) = {kernel_object_get_object, kernel_object_get_methods};

void kernel_methods_init(void) {
	/* Seal kernel methods */
	kernel_methods[0] = __builtin_memcap_seal(kernel_methods[0], __builtin_memcap_global_data_get());
	kernel_methods[1] = __builtin_memcap_seal(kernel_methods[1], __builtin_memcap_global_data_get());
}

void * _syscall_get_kernel_methods(void) {
	return kernel_methods;
}

/* Create an object for the kernel service */
void * _syscall_get_kernel_object(void) {
	sealed_data_t * cb = kernel_calloc(1, sizeof(sealed_data_t));
	cb->process_nb = task_create_bare();
	if(cb->process_nb < 0) {
		kernel_freeze();
	}
	cb->stack = kernel_cp2_exception_framep[cb->process_nb].cf_c11;
	cb->stack_size = kernel_exception_framep[cb->process_nb].mf_sp; //TODO:hack
	
	kernel_cp2_exception_framep[cb->process_nb].cf_c0 = __builtin_memcap_global_data_get();
	
	/* Kernel does not need constructors */
	cb->object = NULL;

	return __builtin_memcap_seal(cb, __builtin_memcap_global_data_get());
}

/* Return the capability needed by service 'n' */ 
static void * object_register_cap(int nb) {
	void * cap = NULL;
	switch(nb) {
		case 1:{}
			#ifdef CONSOLE_malta
				#define	UART_BASE	0x180003f8
				#define	UART_SIZE	0x40


			#elif defined(CONSOLE_altera)
				#define	UART_BASE	0x7f000000
				#define	UART_SIZE	0x08
			#else
			#error UART type not found
			#endif
			cap = cheri_getdefault();
			cap = cheri_setoffset(cap,
			    mips_phys_to_uncached(UART_BASE));
			cap = cheri_setbounds(cap, UART_SIZE);
			break;
		default:{}
	}
	return cap;
}

/* Register a module a service 'nb' */
static int object_register_core(int nb, int flags, void **methods, int methods_nb, void *data_cap) {
	if(bindarr[nb].proc != -1) {
		KERNEL_ERROR("port already used");
		return -1;
	}
	
	/* Seal the methods */
	void ** meths = kernel_malloc(methods_nb*sizeof(void *));
	if(meths == NULL) {
		KERNEL_ERROR("malloc failed");
		return -1;
	}
	
	void * seal = __builtin_memcap_offset_set(__builtin_memcap_global_data_get(), nb);
	
	void * ctor = __builtin_memcap_seal(methods[0], seal);
	void * dtor = __builtin_memcap_seal(methods[1], seal);
	for(int i=0; i<methods_nb; i++) {
		meths[i] = __builtin_memcap_seal(methods[i+2], seal);
	}

	bindarr[nb].proc = kernel_curr_proc;
	bindarr[nb].ctor = ctor;
	bindarr[nb].dtor = dtor;
	bindarr[nb].methods = meths;
	bindarr[nb].data_cap  = data_cap;
	
	if(flags) {
		bindarr[nb].ready = 1;
	}
	
	/* Give the service a specific capability if needed */
	kernel_cp2_exception_framep_ptr->cf_c3 = object_register_cap(nb);
	
	return 0;
}

int object_register(int nb, int flags, void * methods, int methods_nb, void * data_cap) {
	int ret = -1;
	switch(nb) {
		case 1:
		case 2:
			ret = object_register_core(nb, flags, methods, methods_nb, data_cap);
			break;
		default:
			KERNEL_ERROR("unknown port");
			kernel_freeze();
	}
	return ret;
}

void kernel_ccall(void) {
	KERNEL_TRACE(__func__, "in");
	
	/* Unseal CCall cs and cb */
	void * cs = kernel_cp2_exception_framep_ptr->cf_c1;
	sealed_data_t * cb = kernel_cp2_exception_framep_ptr->cf_c2;
	char * all_mem = __builtin_memcap_global_data_get();
	int otype = __builtin_memcap_type_get(cs);
	cs = __builtin_memcap_unseal(cs, all_mem + otype);
	cb = __builtin_memcap_unseal(cb, all_mem + otype);

	/* Set-up the CCallee process */
	kernel_procs[cb->process_nb].runnable = 1;
	kernel_procs[cb->process_nb].ccaller = kernel_curr_proc;
	
	/* Set pc */
	kernel_cp2_exception_framep[cb->process_nb].cf_pcc = cs;
	kernel_exception_framep[cb->process_nb].mf_pc = __builtin_memcap_offset_get(cs);
	kernel_cp2_exception_framep[cb->process_nb].cf_c12 = cs;
	
	/* Set object */
	kernel_cp2_exception_framep[cb->process_nb].cf_idc = cb->object;
	
	/* Set stack */
	kernel_cp2_exception_framep[cb->process_nb].cf_c11 = cb->stack;
	kernel_exception_framep[cb->process_nb].mf_sp = cb->stack_size;
	
	/* Copy arguments */
	kernel_cp2_exception_framep[cb->process_nb].cf_c3 =
	  kernel_cp2_exception_framep[kernel_curr_proc].cf_c3;
	kernel_exception_framep[cb->process_nb].mf_a0 =
	  kernel_exception_framep[kernel_curr_proc].mf_a0;
	kernel_exception_framep[cb->process_nb].mf_a1 =
	  kernel_exception_framep[kernel_curr_proc].mf_a1;
	  
	kernel_procs[kernel_curr_proc].runnable = 0; /* Force synchronous mode for now */
	kernel_reschedule();
}

void kernel_creturn(void) {
	KERNEL_TRACE(__func__, "in");
	int ccaller = kernel_procs[kernel_curr_proc].ccaller;
	kernel_procs[kernel_curr_proc].runnable = 0;
	kernel_procs[ccaller].runnable = 1;
	
	/* Copy return values */
	kernel_cp2_exception_framep[ccaller].cf_c3 =
	   kernel_cp2_exception_framep[kernel_curr_proc].cf_c3;
	kernel_exception_framep[ccaller].mf_v0 =
	   kernel_exception_framep[kernel_curr_proc].mf_v0;
	kernel_exception_framep[ccaller].mf_v1 =
	   kernel_exception_framep[kernel_curr_proc].mf_v1;
	   
	kernel_skip_pid(ccaller);
	kernel_reschedule();
}
