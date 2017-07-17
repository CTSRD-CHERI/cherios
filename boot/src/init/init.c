/*-
 * Copyright (c) 2016 Robert N. M. Watson
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2016 SRI International
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
#include "plat.h"
#include "misc.h"
#include "init.h"
#include "object.h"
#include "stdio.h"
#include "namespace.h"
#include "utils.h"
#include "string.h"
#include "cprogram.h"
#include "thread.h"
#include "tmpalloc.h"
#include "assert.h"
#include "nano/nanokernel.h"

#define B_FS 0
#define B_SO 0
#define B_ZL 1
#define B_T1 0
#define B_T2 0
#define B_T3 0

#define B_ENTRY(_type, _name, _arg, _daemon, _cond) \
	{_type,	_cond, _name, _arg, _daemon, 0, NULL},
#define B_DENTRY(_type, _name, _arg, _cond) \
	 B_ENTRY(_type, _name, _arg, 1, _cond)
#define B_PENTRY(_type, _name, _arg, _cond) \
	 B_ENTRY(_type, _name, _arg, 0, _cond)
#define B_FENCE \
	{m_fence, 1, NULL, 0, 0, 0, NULL},

init_elem_t init_list[] = {
  /*
   * The namespace-mgr and mem-mgr and proc_manager are mutually dependent.
   *
   * - the namespace-mgr needs to allocate memory for tracking ids,
   *   and hence its libuser needs the id of the mem-mgr service
   *
   * - the mem-mgr is a service that needs to announce itself, and
   *   hence it needs to know the id of the namespace-mgr to send the
   *   announcement.
   *
   * - the mem-mgr spawns a worker thread to handle TLB misses. This needs the process manager.
   *
   * - the process manager needs to do allocation, which requires mem-mgr
   *
   * The current approach to cutting this recursive knot is via the
   * following:
   *
   * - the namespace-mgr is initialized with enough memory to track at
   *   least one service, i.e. enough to last it until the first
   *   service announcement it receives.  by convention, it will be
   *   the first service started. Init will allocate memory for it. // NOTE: this is currently not true
   *
   * - The process manager is started second. It will be passed the remainging pool from init and announce itself
   *   to the namespace manager
   *
   * - by convention, the memory-mgr will be the third service
   *   started.  it will be passed the id of the namespace-mgr service
   *   as an initialization argument.  its very first action will be to
   *   announce itself to the namespace-mgr.
   *
   * - the init process will query the namespace-mgr for the number of
   *   registered services.  it will start the remaining services once
   *   it can be sure that the mem-mgr service has registered itself. // NOTE: Not needed. We check every registration
   *
   */
	B_DENTRY(m_namespace,	"namespace.elf",	0,	1)
    B_DENTRY(m_proc,     "proc.elf", 0, 1)
	B_DENTRY(m_memmgt,	"memmgt.elf",		0, 	1)
	B_DENTRY(m_uart,	"uart.elf",		0,	1)
	B_DENTRY(m_core,	"sockets.elf",		0,	B_SO)
	B_DENTRY(m_core,	"zlib.elf",		0,	B_ZL)
	B_DENTRY(m_core,	"virtio-blk.elf",	0,	B_FS)
	B_FENCE
	B_PENTRY(m_fs,		"fatfs.elf",		0,	B_FS)
	B_FENCE
	B_PENTRY(m_user,	"hello.elf",		0,	1)
	B_FENCE
	B_DENTRY(m_user,	"test1b.elf",		0,	B_T1)
	B_PENTRY(m_user,	"prga.elf",		1,	B_SO)
	B_PENTRY(m_user,	"prga.elf",		2,	B_SO)
	B_PENTRY(m_user,	"zlib_test.elf",	0,	B_ZL)
	B_PENTRY(m_user,	"test1a.elf",		0,	B_T1)
	B_PENTRY(m_user,	"test2a.elf",		0,	B_T2)
	B_PENTRY(m_user,	"test2b.elf",		0,	B_T2)
    B_PENTRY(m_user,    "churn.elf",        0,  0)
    B_PENTRY(m_secure,    "foundation_test.elf", 0, 0)

#if 0
	#define T3(_arg) \
	B_PENTRY(m_user,	"test3.elf",		_arg,	B_T3)
	T3(16) T3(17) T3(18) T3(19)
	T3(20) T3(21) T3(22) T3(23) T3(24) T3(25) T3(26) T3(27) T3(28) T3(29)
	T3(30) T3(31) T3(32) T3(33) T3(34) T3(35) T3(36) T3(37) T3(38) T3(39)
	T3(40) T3(41) T3(42) T3(43) T3(44) T3(45) T3(46) T3(47) T3(48) T3(49)
	T3(50) T3(51) T3(52) T3(53) T3(54) T3(55) T3(56) T3(57) T3(58) T3(59)
	T3(60) T3(61) T3(62) T3(63) T3(64) T3(65) T3(66) T3(67) T3(68) T3(69)
	T3(70) T3(71) T3(72) T3(73) T3(74) T3(75) T3(76) T3(77) T3(78) T3(79)
#endif

	{m_fence, 0, NULL, 0, 0, 0, NULL}
};

const size_t init_list_len = countof(init_list);

ALLOCATE_PLT_NANO

static void print_build_date(void) {
	int filelen=0;
	char * date = load("t1", &filelen);
	if(date == NULL) {
		printf("%s failed\n", __func__);
		return;
	}
	date[filelen-1] = '\0';
	printf("%s\n", date);
}

static void print_init_info(init_info_t * init_info) {
	CHERI_PRINT_CAP(init_info);

	if (init_info) {
		CHERI_PRINT_CAP(init_info->nano_if);
		CHERI_PRINT_CAP(init_info->nano_default_cap);
	}
}

extern char __nano_size;

static memmgt_init_t memmgt_init;
static procman_init_t procman_arg;

Elf_Env env;

/* Return the capability needed by the activation */
static void * get_act_cap(module_t type, init_info_t* info) {
    switch(type) {
        case m_uart:
            return info->uart_cap;
        case m_memmgt:

            memmgt_init.nano_default_cap = info->nano_default_cap;
            memmgt_init.nano_if = info->nano_if;
            return &memmgt_init;

        case m_fs:{}
        //TODO get this from memmgt.
            cap_pair pair;
			mmap_new(FS_PHY_BASE, FS_PHY_SIZE,
                            CHERI_PERM_LOAD | CHERI_PERM_STORE,
                            MAP_ANONYMOUS | MAP_SHARED | MAP_PHY, &pair);
            return pair.data;
        case m_proc:
            procman_arg.nano_default_cap = info->nano_default_cap;
            procman_arg.nano_if = info->nano_default_cap;
            return &procman_arg;
        case m_namespace:
        case m_core:
        case m_user:
        case m_fence:
        default:
            return NULL;
    }
}

static capability load_check(const char* name) {
    int filelen=0;
    capability addr = load(name, &filelen);
    if(!addr) {
        printf("Could not read file %s", name);
        assert(0);
    }
    return addr;
}

static void load_modules(init_info_t * init_info) {
    /* This got a little complicated and has been taken out the loop */

    size_t i=0;


    init_elem_t * namebe = init_list + i;
    assert(namebe->type == m_namespace);

    i++;

    init_elem_t * procbe = init_list + i;
    assert(procbe->type == m_proc);

    i++;

    init_elem_t * memgtbe = init_list + i;
    assert(memgtbe->type == m_memmgt);

    i++;


    /* Namespace */
    namebe->ctrl =
            simple_start(&env, namebe->name, load_check(namebe->name), namebe->arg, get_act_cap(m_namespace, init_info));


    namespace_init(syscall_act_ctrl_get_ref(namebe->ctrl));

    /* Proc */

    capability proc_file = load_check(procbe->name);

    /* We have to load this file early as once we hand over the file we will not be able to allocate */
    /* Order VERY important here */
    capability memmgt_file = load_check(memgtbe->name);

    procbe->ctrl =
            simple_start(&env, procbe->name, proc_file, procbe->arg, get_act_cap(m_proc, init_info));

    /* FIXME technically a bit of a race. This global will be read by proc so it needs to be set before context switch */

    procman_arg.pool_from_init = get_remaining();
    /* Wait for registration */

    printf("Waiting for proc manager to register \n");
    while(namespace_get_ref(namespace_num_proc_manager) == NULL) {
        nssleep(3);
    }printf("proc manager registered \n");

    /* Memmgt */

    startup_desc_t desc;
    desc.arg = memgtbe->arg;
    desc.carg = get_act_cap(m_memmgt, init_info);
    desc.stack_args = NULL;
    desc.stack_args_size = 0;
    /* This version allows the process to spawn new threads */
    memgtbe->ctrl = thread_start_process(thread_create_process(memgtbe->name, memmgt_file, 0), &desc);

    /* Wait for registration */

    printf("Waiting for memory manager to register \n");
    while(namespace_get_ref(namespace_num_memmgt) == NULL) {
        nssleep(3);
    }
    printf("memory manager registered \n");

    /* We no longer have our pool. But now we can use virtual memory */

    env.alloc = &mmap_based_alloc;
    env.free = &mmap_based_free;

    /* Now load the rest */

	for(; i<init_list_len; i++) {
		int cnt;
        init_elem_t * be = init_list + i;

		if(be->cond == 0)
			continue;

		if(be->type == m_fence) {
			/* nssleep(3); */
			continue;
		}

        void *carg = get_act_cap(be->type, init_info);
        capability  addr = load_check(be->name);

        desc.arg = be->arg;
        desc.carg = carg;
        desc.stack_args = NULL;
        desc.stack_args_size = 0;
        /* This version allows the process to spawn new threads */
        be->ctrl = thread_start_process(thread_create_process(be->name, addr, be->type == m_secure), &desc);

		printf("Module ready: %s\n", be->name);
	}
}

static capability pool[POOL_SIZE/sizeof(capability)];

int main(init_info_t * init_info) {
    init_nano_kernel_if_t(init_info->nano_if,init_info->nano_default_cap);

	stats_init();

    env.free = &tmp_free;
    env.alloc = &tmp_alloc;
    env.printf = &printf;
    env.vprintf = &printf;
    env.memcpy = &memcpy;

    printf("Init loaded\n");

	/* Initialize the memory pool. */
	init_tmp_alloc((cap_pair){.data = pool, .code = rederive_perms(pool, cheri_getpcc())});

	/* Print fs build date */
	print_build_date();
	print_init_info(init_info);

	/* Load modules */
	load_modules(init_info);

    printf("All modules loaded! waiting for finish...\n");

	while(acts_alive(init_list, init_list_len)) {
        nssleep(10);
	}

	printf(KBLD"Only daemons are alive. System shutown."KRST"\n");
	stats_display();

    syscall_shutdown(REBOOT);
}
