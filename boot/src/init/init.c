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
#include "capmalloc.h"
#include "../../../cherios/kernel/include/sched.h"
#include "crt.h"

#define B_FS 0
#define B_SO 0
#define B_ZL 0
#define B_T1 0
#define B_T2 0
#define B_T3 0

#define B_BENCH 0

#define B_BENCH_MS      0
#define B_BENCH_CALLS   0
#define B_BENCH_EXPS    0

const char* nginx_args[] = {"nginx",NULL};
#define NGINX_ARGS_L 1

#ifdef HARDWARE_qemu
    #include "malta_virtio_mmio.h"

    #define BLK_MMIO_BASE   VIRTIO_MMIO_MMAP_BASE
    #define BLK_MMIO_SIZE   VIRTIO_MMIO_SIZE
    #define BLK_MMIO_IRQ    VIRTIO_MMIO_IRQ

    #define FS_ELF          "fatfs.elf"
    #define BLK_ELF         "virtio-blk.elf"
#else
    #include "alteraSD.h"

    #define BLK_MMIO_BASE   ALTERA_SD_BASE
    #define BLK_MMIO_SIZE   ALTERA_SD_SIZE
    #define BLK_MMIO_IRQ    0

    #define FS_ELF          "fatfs.elf"
    #define BLK_ELF         "alteraSD.elf"

#endif

#define B_ENTRY(_type, _name, _arg, _daemon, _cond) \
	{_type,	_cond, _name, _arg, _daemon, 0, NULL},
#define B_DENTRY(_type, _name, _arg, _cond) \
	 B_ENTRY(_type, _name, _arg, 1, _cond)
#define B_PENTRY(_type, _name, _arg, _cond) \
	 B_ENTRY(_type, _name, _arg, 0, _cond)
#define B_FENCE \
	{m_fence, 1, NULL, 0, 0, 0, NULL},
#define B_WAIT_FOR(X) \
    {m_fence, 1, NULL, X, 0, 0, NULL},

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
   *   all services, i.e. it will not call malloc. By convention, it will be
   *   the first service started. Init will allocate memory for it.
   *
   * - The process manager is started second. Init will alloc space for it. It will be passed the remaining
   *   allocation pool from init and announce itself to the namespace manager.
   *
   * - by convention, the memory-mgr will be the third service
   *   started.  it will be passed the id of the namespace-mgr service
   *   as an initialization argument.  its very first action will be to
   *   announce itself to the namespace-mgr and the kernel for virtual memory trap handling.
   *
   * - the init process will query the namespace-mgr until these core services are all registered and only then
   *   continue with loading.
   *
   * Many other services are quite critical, but there should be no mutual dependencies. However, if an application
   * needs one of these services (e.g. the type manager), it should spin until it gets a result from the namespace
   * manager
   */

	B_DENTRY(m_namespace,	"namespace.elf",	0,	1)
    B_DENTRY(m_proc,     "proc.elf", 0, 1)
	B_DENTRY(m_memmgt,	"memmgt.elf",		0, 	1)
// NOTE: IDLE processes are loaded here
    B_DENTRY(m_secure,    "libsocket.elf", 0, 1)
    B_WAIT_FOR(namespace_num_lib_socket)
    B_DENTRY(m_uart,	"uart.elf",		0,	1)      // Needed for stdout so bring up asap. This needs the link server for libsocket
    B_DENTRY(m_user,    "activation_events.elf", 0, 1)
    B_DENTRY(m_dedup,    "dedup.elf", 0 ,1)
    B_DENTRY(m_dedup_init, "dedup_init.elf", 0, 1)
    B_WAIT_FOR(namespace_num_dedup_service)
    B_DENTRY(m_tman, "type_manager.elf",0,1)
    B_WAIT_FOR(namespace_num_tman)
    B_DENTRY(m_user, "calls.elf", 0, B_BENCH && B_BENCH_CALLS)
    B_DENTRY(m_user, "message_send.elf", 0, B_BENCH && B_BENCH_MS)
    B_DENTRY(m_user, "exceptions.elf", 0, B_BENCH && B_BENCH_EXPS)
//  B_DENTRY(m_core,	"sockets.elf",		0,	B_SO)
	B_DENTRY(m_core,	"zlib.elf",		0,	B_ZL)
	B_DENTRY(m_virtblk,	BLK_ELF,	0,	1)
    B_DENTRY(m_secure | m_user, "block_cache.elf", 0, 1)
    B_WAIT_FOR(namespace_num_blockcache)
    B_PENTRY(m_virtnet, "lwip.elf", 0, 1 && BUILD_WITH_NET)
	B_FENCE
	B_DENTRY(m_fs,	FS_ELF	,		0,	1)
	B_FENCE
	B_PENTRY(m_user,	"hello.elf",		0,	1)
	B_WAIT_FOR(namespace_num_fs)
#if(BUILD_WITH_NET)
    B_WAIT_FOR(namespace_num_tcp)
#endif
	B_DENTRY(m_user,	"test1b.elf",		0,	B_T1)
	B_PENTRY(m_user,	"prga.elf",		1,	B_SO)
	B_PENTRY(m_user,	"prga.elf",		2,	B_SO)
	B_PENTRY(m_user,	"zlib_test.elf",	0,	B_ZL)
	B_PENTRY(m_user,	"test1a.elf",		0,	B_T1)
	B_PENTRY(m_user,	"test2a.elf",		0,	B_T2)
	B_PENTRY(m_user,	"test2b.elf",		0,	B_T2)
    B_PENTRY(m_user,    "exception_test.elf", 0, !B_BENCH)
    B_PENTRY(m_user, "unsafe_test.elf", 0, !B_BENCH)
    B_PENTRY(m_user,    "dedup_test.elf", 0, !B_BENCH)
    B_PENTRY(m_user,    "socket_test.elf", 0 ,!B_BENCH)
    B_PENTRY(m_user, "fs_test.elf", 0, !B_BENCH)
//    B_DENTRY(m_user, "server.elf", 0, 1)
//    B_PENTRY(m_user, "client.elf", 0, 1)
    B_PENTRY(m_user,    "churn.elf",        0,  0)
    B_PENTRY(m_secure,    "foundation_test.elf", 0, !B_BENCH)
// B_PENTRY(m_nginx | m_secure, "nginx.elf",NGINX_ARGS_L,1 && BUILD_WITH_NET)
    B_PENTRY(m_user, "top.elf", 0, !B_BENCH)
    B_PENTRY(m_user, "nc_shell.elf", 0, BUILD_WITH_NET)
    B_PENTRY(m_user, "snake.elf",0, BUILD_WITH_NET)
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

// ALLOCATE_PLT_NANO

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
static act_kt dedup_act;

Elf_Env env;

/* Return the capability needed by the activation */
static void * get_act_cap(module_t type, init_info_t* info) {
    type = (type & ~m_secure);
    switch(type) {
        case m_uart:
            return info->uart_cap;
        case m_memmgt:

            memmgt_init.nano_default_cap = info->nano_default_cap;
            memmgt_init.nano_if = info->nano_if;
            memmgt_init.mop_sealing_cap = info->mop_sealing_cap;
            memmgt_init.mop_signal_flag = 0;

            return &memmgt_init;

        case m_fs:{}
            if(BLK_MMIO_BASE == 0) return NULL;
            cap_pair pair;
            get_physical_capability(BLK_MMIO_BASE, BLK_MMIO_SIZE, 1, 0, own_mop, &pair);
            return pair.data;
        case m_virtnet:
            return NULL;
        case m_proc:
            procman_arg.nano_default_cap = info->nano_default_cap;
            procman_arg.nano_if = info->nano_if;
            procman_arg.sealer = info->top_sealing_cap;
            return &procman_arg;
        case m_nginx:
            return nginx_args;
        case m_dedup_init:
            return dedup_act;
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

#define PAUSE sleep(MS_TO_CLOCK(100))

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

    image namespace_im;
    image proc_im;

    /* Namespace */
    namebe->ctrl =
            simple_start(&env, namebe->name, load_check(namebe->name),
                         namebe->arg, get_act_cap(m_namespace, init_info), NULL, &namespace_im);


    namespace_init(syscall_act_ctrl_get_ref(namebe->ctrl));

    /* Proc */

    capability proc_file = load_check(procbe->name);

    /* We have to load this file early as once we hand over the file we will not be able to allocate */
    /* Order VERY important here */
    capability memmgt_file = load_check(memgtbe->name);

    procbe->ctrl =
            simple_start(&env, procbe->name, proc_file, procbe->arg, get_act_cap(m_proc, init_info), NULL, &proc_im);

    /* No longer a race, procman while wait for this */

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
    desc.cpu_hint = 0;
    desc.flags = STARTUP_NO_DEDUP;

    /* This version allows the process to spawn new threads */
    memgtbe->ctrl = thread_start_process(thread_create_process(memgtbe->name, memmgt_file, 0), &desc);

    /* Wait for registration */

    printf("Waiting for memory manager to register \n");
    while(namespace_get_ref(namespace_num_memmgt) == NULL) {
        nssleep(3);
    }

    try_init_memmgt_ref();

    printf("memory manager registered \n");

    printf("Wait for mop pass back\n");
    while (memmgt_init.mop_signal_flag == 0) {
        nssleep(3);
    }

    /* This is the base mop for our system. We should create two children, one for use by init, one for use by proc_man
     * proc_man will then furthere subdivide for all the processes it creates */

    mop_t mop = (mop_t)memmgt_init.base_mop;

    /* We need to do this as we had no mop when created */
    mmap_set_mop(mop);

    printf("Creating mop for proc man\n");
    // TODO eventually capmalloc will do this nicely */
    res_t space_for_mop = cap_malloc(MOP_REQUIRED_SPACE);

    CHERI_PRINT_CAP(space_for_mop);

    ERROR_T(mop_t) mop_t_or_er = mem_makemop_debug(space_for_mop, mop, "proc");

    if(!IS_VALID(mop_t_or_er)) {
        assert_int_ex(-mop_t_or_er.er, == , 0);
    }

    mop_t mop_for_proc = mop_t_or_er.val;

    /* Send the mop */
    printf("Passing mop to proc man\n");
    message_send_c(0,0,0,0,mop_for_proc,NULL,NULL,NULL,syscall_act_ctrl_get_ref(procbe->ctrl), SEND, 3);

    /* We no longer have our pool. But now we can use virtual memory */

    printf("Core load finished. Loading other processes\n");
    env.alloc = &mmap_based_alloc;
    env.free = (typeof(env.free))&mmap_based_free;
    env.handle = mop;

    /* Now load an idle activation for each core */

    const char* idle_name = "idle.elf";
    capability  idle_addr = load_check(idle_name);

    for(size_t idle_id = 0; idle_id < SMP_CORES; idle_id++) {
        printf("Creating idle activation %lx\n", idle_id);
        desc.arg = idle_id;
        desc.carg = init_info->idle_init.queue_fill_pre[idle_id];
        desc.stack_args = NULL;
        desc.stack_args_size = 0;
        desc.flags = STARTUP_BASIC;
        __unused act_control_kt ctrl = thread_start_process(thread_create_process(idle_name,idle_addr,0), &desc);
    }

    /* Now load the rest (for fun on a different CPU) */

	for(; i<init_list_len; i++) {
        init_elem_t * be = init_list + i;

		if(be->cond == 0)
			continue;

		int secure = (be->type & m_secure) == m_secure;
		module_t type = be->type & ~m_secure;

		if(type == m_fence) {
            if(be->arg) {
                while(namespace_get_ref((int)be->arg) == NULL) {
                    PAUSE;
                }
            } else {
                PAUSE;
            }
			continue;
		}

        void *carg = get_act_cap(type, init_info);
        capability  addr = load_check(be->name);

        desc.arg = be->arg;
        desc.carg = carg;
        desc.stack_args = NULL;
        desc.stack_args_size = 0;
        desc.cpu_hint = SMP_CORES-1;
        desc.flags = STARTUP_NONE;
        desc.inv = NULL;

        if(type == m_virtblk || type == m_virtnet) desc.cpu_hint = 0; // Some things really like to scheduled on core0 for interrupts

        /* This version allows the process to spawn new threads */
        be->ctrl = thread_start_process(thread_create_process(be->name, addr, secure), &desc);

        if(type == m_dedup) {
            dedup_act = syscall_act_ctrl_get_ref(be->ctrl);
        }
		printf("Module ready: %s\n", be->name);
	}
}

// Init will not have a TLS segment provided
char tls_segment[0x2000];

capability
crt_init_globals_init()
{
    // This works

    void *gdc = cheri_getdefault();
    void *pcc = cheri_setoffset(cheri_getpcc(), 0);

    uint64_t text_start;
    uint64_t data_start;
    uint64_t tls_start;

    cheri_dla(__text_segment_start, text_start);
    cheri_dla(__cap_table_local_start, data_start); // This seems to come data now
    cheri_dla(__cap_table_local_start, tls_start); // A guess that this comes before tbss and tdata

    __unused capability text_segment = (char*)pcc + text_start;
    __unused capability data_segment = (char*)gdc + data_start;

    capability segment_table[5];

    // These are all set up by the linker script
    segment_table[0] = NULL;
    segment_table[2] = (char*)pcc + text_start;
    segment_table[3] = (char*)gdc + data_start;
    segment_table[4] = (char*)gdc + tls_start;

    // Get something usable
    uint64_t table_start = 0, reloc_start = 0, reloc_end = 0;
    cheri_dla(__cap_table_start, table_start);
    cheri_dla(__start___cap_relocs, reloc_start);
    cheri_dla(__stop___cap_relocs, reloc_end);

    capability cgp = cheri_setoffset(gdc, table_start);
    cheri_setreg(25, cgp);

    crt_init_common(segment_table,
            (struct capreloc *)((char*)gdc + reloc_start),
                    (struct capreloc *)((char*)gdc + reloc_end),
                            RELOC_FLAGS_TLS);

    cheri_setreg(25, &__cap_table_start);

    // Provide our own tls segment

    segment_table[4] = (capability)tls_segment;
    capability local_captab = cheri_setbounds(segment_table[4],cheri_getlen(&__cap_table_local_start));

    cheri_setreg(26, local_captab);

    crt_init_common(segment_table, (struct capreloc *)((char*)gdc + reloc_start),
                    (struct capreloc *)((char*)gdc + reloc_end), 0);

    // We return a capability to our tls_segment. Other threads will have this provided by the linker
    return local_captab;
}

static capability pool[POOL_SIZE/sizeof(capability)];

int main(init_info_t * init_info, capability pool_auth_cap) {

    env.free = &tmp_free;
    env.alloc = &tmp_alloc;
    env.printf = &printf;
    env.vprintf = &vprintf;
    env.memcpy = &memcpy;

    printf("Init loaded\n");

	/* Initialize the memory pool. */
	init_tmp_alloc((cap_pair){.data = pool, .code = rederive_perms(pool, pool_auth_cap)});

	/* Print fs build date */
	print_build_date();
	print_init_info(init_info);

	/* Load modules */
	load_modules(init_info);

    printf("All modules loaded! waiting for finish...\n");

    acts_wait_for_finish(init_list, init_list_len);

	printf(KBLD"Only daemons are alive. System shutown."KRST"\n");

    syscall_shutdown(REBOOT);

    assert(0 && "Should not get past reboot");
}
