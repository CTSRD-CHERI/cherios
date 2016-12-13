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

#include "plat.h"
#include "misc.h"
#include "init.h"
#include "object.h"
#include "stdio.h"

#define B_FS 1
#define B_SO 1
#define B_ZL 1
#define B_T1 0
#define B_T2 0
#define B_T3 0
#define B_MI 1

#define B_ENTRY(_type, _name, _arg, _daemon, _cond) \
	{_type,	_cond, _name, _arg, _daemon, 0, NULL},
#define B_DENTRY(_type, _name, _arg, _cond) \
	 B_ENTRY(_type, _name, _arg, 1, _cond)
#define B_PENTRY(_type, _name, _arg, _cond) \
	 B_ENTRY(_type, _name, _arg, 0, _cond)
#define B_FENCE \
	{m_fence, 1, NULL, 0, 0, 0, NULL},

init_elem_t init_list[] = {
	B_DENTRY(m_memmgt,	"memmgt.elf",		0, 	1)
	B_FENCE
	B_DENTRY(m_namespace,	"namespace.elf",	0,	1)
	B_DENTRY(m_uart,	"uart.elf",		0,	1)
	B_FENCE
	B_PENTRY(m_user,	"qsort.elf",		0, 	B_MI)
	B_PENTRY(m_user,	"spam.elf",		0, 	B_MI)
	B_PENTRY(m_user,	"CRC32.elf",		0, 	B_MI)
	B_PENTRY(m_user,	"stringsearch.elf",		0, 	B_MI)
	//B_DENTRY(m_core,	"sockets.elf",		0,	B_SO)
	//B_DENTRY(m_core,	"zlib.elf",		0,	B_ZL)
	//B_DENTRY(m_core,	"virtio-blk.elf",	0,	B_FS)
	//B_DENTRY(m_core,	"test1b.elf",		0,	B_T1)
	//B_FENCE
	//B_PENTRY(m_fs,		"fatfs.elf",		0,	B_FS)
	B_FENCE
	//B_PENTRY(m_user,	"hello.elf",		0,	1)
	//B_FENCE
	//B_PENTRY(m_user,	"prga.elf",		1,	B_SO)
	//B_PENTRY(m_user,	"prga.elf",		2,	B_SO)
	//B_PENTRY(m_user,	"zlib_test.elf",	0,	B_ZL)
	//B_PENTRY(m_user,	"test1a.elf",		0,	B_T1)
	//B_PENTRY(m_user,	"test2a.elf",		0,	B_T2)
	//B_PENTRY(m_user,	"test2b.elf",		0,	B_T2)

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

void print_build_date(void) {
	int filelen=0;
	char * date = load("t1", &filelen);
	if(date == NULL) {
		printf("%s failed\n", __func__);
		return;
	}
	date[filelen-1] = '\0';
	printf("%s\n", date);
}

static void load_modules(void) {
	static void * c_memmgt = NULL;

	for(size_t i=0; i<init_list_len; i++) {
		init_elem_t * be = init_list + i;
		if(be->cond == 0) {
			continue;
		}
		if(be->type == m_fence) {
			nssleep(3);
			continue;
		}
		be->ctrl = load_module(be->type, be->name, be->arg, NULL);
		printf("Loaded module %s\n", be->name);
		switch(init_list[i].type) {
		case m_memmgt:
			nssleep(3);
			c_memmgt = be->ctrl;
			init_alloc_enable_system(be->ctrl);
			break;
		case m_namespace:
			nssleep(3);
			/* glue memmgt to namespace */
			glue_memmgt(c_memmgt, be->ctrl);
			break;
		default:{}
		}
	}
}

int init_main() {
  	stats_init();

	printf("Init loaded\n");

	/* Initialize the memory pool. */
	init_alloc_init();

	/* Print fs build date */
	printf("Init:C\n");
	print_build_date();

	/* Load modules */
	printf("Init:F\n");
	load_modules();

	printf("Init:Z\n");

	while(acts_alive(init_list, init_list_len)) {
		ssleep(0);
	}

	printf(KBLD"Only daemons are alive. System shutown."KRST"\n");
	stats_display();
	hw_reboot();
}
