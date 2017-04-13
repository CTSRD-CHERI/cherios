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

#include "klib.h"
#include "nanokernel.h"
#include "cp0.h"

ALLOCATE_PLT_NANO

#define printf kernel_printf

/* Use linker allocated memory to store boot-info. */
static boot_info_t boot_info;

int cherios_main(context_t own_con, nano_kernel_if_t* interface, capability def_data, boot_info_t* info) {
	kernel_puts("Kernel Hello world\n");
	kernel_setup_trampoline();
	init_nano_kernel_if_t(interface, def_data);

    /*
    * Copy boot_info from boot-loader memory to our own before
    * processing it.
    *
    * TODO: check that the expected size matches.
    */

	memcpy(&boot_info, info, sizeof(boot_info));
	context_t init_context = act_init(own_con, &boot_info);

	KERNEL_TRACE("kernel", "Going into exception handling mode");

	// We re-use this context as an exception context. Maybe we should create a proper one?
	kernel_exception(init_context, own_con); // Only here can we start taking exceptions, otherwise we crash horribly
	kernel_panic("exception handler should never return");
}
