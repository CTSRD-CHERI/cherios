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
#include "uart.h"

ALLOCATE_PLT_NANO

#define printf kernel_printf

/* Use linker allocated memory to store boot-info. */
static init_info_t init_info;
sealing_cap def_seal_cap;

int cherios_main(nano_kernel_if_t* interface,
				 capability def_data,
				 context_t own_context,
                 sealing_cap sealer,
				 size_t init_base,
                 size_t init_entry) {
	/* This MUST be called before trying to use the nano kernel, which we will need to do in order
	 * to get access to the phy mem we need */

	init_nano_kernel_if_t(interface, def_data);
    set_sealing_cap(sealer);
	/* Get the capability for the uart. We should save this somewhere sensible */
	capability  cap_for_uart = get_phy_cap(get_book(), uart_base_phy_addr, uart_base_size, 0);
	set_uart_cap(cap_for_uart);

	kernel_puts("Kernel Hello world\n");

    init_fast_critical_section();

    CHERI_PRINT_CAP(interface);
    CHERI_PRINT_CAP(def_data);
    CHERI_PRINT_CAP(own_context);
    //CHERI_PRINT_CAP(reservation);
    CHERI_PRINT_CAP(sealer);
    kernel_printf("init_base: %lx. entry: %lx\n", init_base, init_entry);

	kernel_setup_trampoline();

	init_info.nano_if = interface;
	init_info.nano_default_cap = def_data;
	init_info.kernel_size = cheri_getlen(cheri_getdefault());
	kernel_assert((init_info.kernel_size & (PAGE_SIZE-1)) == 0);
    init_info.uart_cap = cap_for_uart;
	init_info.uart_page = uart_base_phy_addr / PAGE_SIZE;

	sched_init();

	context_t init_context = act_init(own_context, &init_info, init_base, init_entry);

	KERNEL_TRACE("kernel", "Going into exception handling mode");

	// We re-use this context as an exception context. Maybe we should create a proper one?
	kernel_exception(init_context, own_context); // Only here can we start taking exceptions, otherwise we crash horribly
	kernel_panic("exception handler should never return");
}
