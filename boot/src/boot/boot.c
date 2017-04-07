/*-
 * Copyright (c) 2016 Robert N. M. Watson
 * Copyright (c) 2016 Hadrien Barral
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

#include "sys/types.h"
#include "boot/boot.h"
#include "cp0.h"
#include "misc.h"
#include "object.h"
#include "string.h"
#include "syscalls.h"

#define B_FS 1
// TODO I question the need for this kind of socket. We have a message passing mechanism with far more fine grained
// TODO semantics. Why do we want to reduce this to a single integer 'port' interface? Currently broken due to use
// TODO of idc.
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

static boot_elem_t boot_list[] = {
	// TODO other end of the hack. The kernel assumes the first activation will be the namespace service
	B_DENTRY(m_namespace,	"namespace.elf",	0,	1)
	B_DENTRY(m_memmgt,	"memmgt.elf",		0, 	1)
	B_DENTRY(m_uart,	"uart.elf",		0,	1)
	B_DENTRY(m_core,	"sockets.elf",		0,	B_SO)
	B_DENTRY(m_core,	"zlib.elf",		0,	B_ZL)
	B_DENTRY(m_core,	"virtio-blk.elf",	0,	B_FS)
	B_DENTRY(m_core,	"test1b.elf",		0,	B_T1)
	B_FENCE
	B_PENTRY(m_fs,		"fatfs.elf",		0,	B_FS)
	B_FENCE
	B_PENTRY(m_user,	"hello.elf",		0,	1)
	B_FENCE
	B_PENTRY(m_user,	"prga.elf",		1,	B_SO)
	B_PENTRY(m_user,	"prga.elf",		2,	B_SO)
	B_PENTRY(m_user,	"zlib_test.elf",	0,	B_ZL)
	B_PENTRY(m_user,	"test1a.elf",		0,	B_T1)
	B_PENTRY(m_user,	"test2a.elf",		0,	B_T2)
	B_PENTRY(m_user,	"test2b.elf",		0,	B_T2)

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

static const size_t boot_list_len = countof(boot_list);

static void print_build_date(void) {
	int filelen=0;
	char * date = load("t1", &filelen);
	if(date == NULL) {
		boot_printf("%s failed\n", __func__);
		return;
	}
	date[filelen-1] = '\0';
	boot_printf("%s\n", date);
}

static void load_modules(void) {

	for(size_t i=0; i<boot_list_len; i++) {
		boot_elem_t * be = boot_list + i;
		if(be->cond == 0) {
			continue;
		}
		if(be->type == m_fence) {
			nssleep(3);
			continue;
		}
		be->ctrl = load_module(be->type, be->name, be->arg);
		switch(boot_list[i].type) {
			case m_memmgt:
				boot_alloc_enable_system(be->ctrl);
				break;
			case m_namespace:
				nssleep(3);
				break;
			default:{}
		}
	}
}

//FIXME we should make sure only the nano kernel can modify the exception vectors
static void install_exception_vectors(void) {
	/* Copy exception trampoline to exception vector */
	char * all_mem = cheri_getdefault() ;
	void *mips_bev0_exception_vector_ptr =
			(void *)(all_mem + MIPS_BEV0_EXCEPTION_VECTOR);
	memcpy(mips_bev0_exception_vector_ptr, &kernel_exception_trampoline,
		   (char *)&kernel_exception_trampoline_end - (char *)&kernel_exception_trampoline);

	void *mips_bev0_ccall_vector_ptr =
			(void *)(all_mem + MIPS_BEV0_CCALL_VECTOR);
	memcpy(mips_bev0_ccall_vector_ptr, &kernel_ccall_trampoline,
		   (char *)&kernel_ccall_trampoline_end - (char *)&kernel_ccall_trampoline);

	/* Invalidate I-cache */
	__asm volatile("sync");

	__asm volatile(
	"cache %[op], 0(%[line]) \n"
	:: [op]"i" ((0b100<<2)+0), [line]"r" (MIPS_BEV0_EXCEPTION_VECTOR & 0xFFFF));
	__asm volatile(
	"cache %[op], 0(%[line]) \n"
	:: [op]"i" ((0b100<<2)+0), [line]"r" (MIPS_BEV0_CCALL_VECTOR & 0xFFFF));

	//FIXME if we want boot exceptions we should install other vectors and not set this here
	cp0_status_bev_set(0);

	/* does not work with kseg0 address, hence the `& 0xFFFF` */
	__asm volatile("sync");
}

// FIXME remove this when we seperate boot and init
__attribute((noinline))
void create_context_hack(reg_frame_t* frame, capability table, capability data) {
	capability store;
	//This actually clobbers a whole lot more, but it clobbers the same things as this function would
	//As long as the function is not inlined this is fine.
	__asm__ __volatile__ (
			"cmove	$c18, %[table]\n"
			"cmove	$c19, %[data]\n"
			"clc	$c1, $zero, 0($c18)\n"
			"cmove	$c2, $c19\n"
			"cmove  $c3, %[frame]\n"
			"li		$a0, 1\n"
			"dla	$t0, 1f\n"
			"cgetpccsetoffset $c17, $t0\n"
			"ccall	$c1, $c2\n"
			"1:clc  $c1, $zero, 64($c18)\n"
			"cmove	$c2, $c19\n"
			"cmove	$c4, %[store]\n"
			"dla	$t0, 1f\n"
			"cgetpccsetoffset $c17, $t0\n"
			"ccall	$c1, $c2\n"
			"1:nop\n"
	:
	: [data]"C"(data), [table]"C"(table), [frame]"C"(frame), [store]"C"(&store)
	: "$c1", "$c2", "$c3", "$c4", "$c17", "$c18", "$c19", "t0", "a0"
	);
}

int cherios_main(capability own_context, capability table, capability data);
int cherios_main(capability own_context, capability table, capability data) {
	/* Init hardware */
	hw_init();

	boot_printf("Hello world\n");

	/* Init bootloader */
	boot_printf("B\n");
	stats_init();
	boot_alloc_init();

	/* Print fs build date */
	boot_printf("C\n");
	print_build_date();

	/* Load and init kernel */
	boot_printf("D\n");

	install_exception_vectors();
	capability entry = load_kernel("kernel.elf");
	capability init_func = cheri_setoffset(cheri_getpcc(), cheri_getoffset(entry) + cheri_getbase(entry));

	reg_frame_t k_frame;
	bzero(&k_frame, sizeof(k_frame));
	// FIXME remove this when we have seperated init and boot
	// FIXME activation to do the rest
	struct boot_hack_t hack;

	k_frame.cf_c0 = cheri_getdefault();
	k_frame.cf_pcc = init_func; //FIXME needs to be executable
	k_frame.cf_c4 = own_context;
	k_frame.cf_c5 = table;
	k_frame.cf_c6 = data;
	k_frame.cf_c7 = (capability)&hack;

	create_context_hack(&k_frame, table, data);
	// This catch 22 only exists here, and can be fixed by seperating boot and init


	boot_printf("D.2\n");

	/* If boot wants to be an activation (really it should create an init activation) it will have to call object_init */

	object_init(hack.self_ctrl, NULL, hack.queue, hack.kernel_if_c);

	/* Interrupts are ON from here */
	boot_printf("E\n");

	/* Switch to syscall print */
	boot_printf_syscall_enable();

	/* Load modules */
	boot_printf("F\n");
	load_modules();

	boot_printf("Z\n");

	while(acts_alive(boot_list, boot_list_len)) {
		ssleep(0);
	}

	boot_printf(KBLD"Only daemons are alive. System shutown."KRST"\n");
	stats_display();
	hw_reboot();
}
