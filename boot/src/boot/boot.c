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
#include "plat.h"

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

typedef void init_t(capability context, capability table, capability data, boot_info_t* bi);

void bootloader_main(capability own_context, capability table, capability data);
void bootloader_main(capability own_context, capability table, capability data) {

	/* Init hardware */
	hw_init();

	/* Initialize elf-loader environment */
	init_elf_loader();

	boot_printf("Boot: loading kernel ...\n");
	capability entry = load_kernel();
	init_t* init_func = (init_t*)cheri_setoffset(cheri_getpcc(), cheri_getoffset(entry) + cheri_getbase(entry));

	boot_printf("Boot: loading init ...\n");
	boot_info_t *bi = load_init();

	install_exception_vectors();

	/* Jumps to the kernel init. This will completely destroy boot and so we can never return here */
	init_func(own_context, table, data, bi);
}