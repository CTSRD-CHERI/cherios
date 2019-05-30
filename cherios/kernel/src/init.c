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

#include <nano/nanotypes.h>
#include "klib.h"
#include "nano/nanokernel.h"
#include "cp0.h"
#include "uart.h"
#include "crt.h"

ALLOCATE_PLT_NANO

#define printf kernel_printf

/* Use linker allocated memory to store boot-info. */
static init_info_t init_info;
capability fpga_cap;
capability int_cap;
if_req_auth_t req_auth_for_activations;

capability
crt_init_globals_kernel()
{
    // This works

    void *gdc = cheri_getdefault();
    void *pcc = cheri_setoffset(cheri_getpcc(), 0);

    uint64_t text_start;
    uint64_t data_start;

    cheri_dla(__text_segment_start, text_start);
    cheri_dla(__data_segment_start, data_start);

    capability text_segment = (char*)pcc + text_start;
    capability data_segment = (char*)gdc + data_start;

    capability segment_table[4];

    // These are all set up by the linker script
    segment_table[0] = NULL;
    segment_table[2] = text_segment;
    segment_table[3] = data_segment;

    // Get something usable
    uint64_t table_start = 0, reloc_start = 0, reloc_end = 0;
    cheri_dla(__cap_table_start, table_start);
    cheri_dla(__start___cap_relocs, reloc_start);
    cheri_dla(__stop___cap_relocs, reloc_end);

    capability cgp = cheri_setoffset(gdc, table_start);
    cheri_setreg(25, cgp);

    crt_init_common(segment_table, (struct capreloc * )((char*)gdc + reloc_start),
                    (struct capreloc * )((char*)gdc + reloc_end), RELOC_FLAGS_TLS);

    cheri_setreg(25, &__cap_table_start);

    return &__cap_table_local_start;
}

int cherios_main(nano_kernel_if_t* interface,
				 capability def_data,
				 context_t own_context,
				 __unused capability plt_auth_cap,
                 capability global_pcc,
                 if_req_auth_t req_auth,
				 size_t init_base,
                 size_t init_entry,
                 size_t init_tls_base) {
	/* This MUST be called before trying to use the nano kernel, which we will need to do in order
	 * to get access to the phy mem we need */

    init_nano_kernel_if_t(interface, def_data, &plt_common_complete_trusting);

    int_cap = get_integer_space_cap();
    req_auth_for_activations = if_req_and_mask(req_auth, NANO_KERNEL_USER_ACCESS_MASK);

	/* Get the capability for the uart. We should save this somewhere sensible */
    page_t* book = get_book();

    /* All pages start out dirty. Give them a clean here */

    size_t page_n = 0;

    do {
        if(book[page_n].status == page_dirty) {
            zero_page_range(page_n);

        }
        page_n += book[page_n].len;
    } while(page_n != BOOK_END);


	capability  cap_for_uart = get_phy_cap(book, uart_base_phy_addr, uart_base_size, 0, 1);

#ifdef HARDWARE_qemu
    fpga_cap = get_phy_cap(book, FPGA_BASE, FPGA_SIZE, 0, 1);
#endif

	set_uart_cap(cap_for_uart);

	kernel_puts("Kernel Hello world\n");

    CHERI_PRINT_CAP(interface);
    CHERI_PRINT_CAP(def_data);
    CHERI_PRINT_CAP(own_context);
    //CHERI_PRINT_CAP(reservation);
    //CHERI_PRINT_CAP(sealer);
    kernel_printf("init_base: %lx. entry: %lx. tls_base: %lx\n", init_base, init_entry, init_tls_base);

	init_info.nano_if = interface;
	init_info.nano_default_cap = def_data;
	init_info.kernel_size = cheri_getlen(cheri_getdefault());
	kernel_assert((init_info.kernel_size & (PAGE_SIZE-1)) == 0);
    init_info.uart_cap = cap_for_uart;
	init_info.uart_page = uart_base_phy_addr / PAGE_SIZE;

	init_info.mop_sealing_cap = get_sealing_cap_from_nano(MOP_SEALING_TYPE);
    init_info.top_sealing_cap = get_sealing_cap_from_nano(PROC_SEALING_TYPE);

    kernel_printf("Initialising Scheduler\n");
	sched_init(&init_info.idle_init);

    kernel_printf("Initialising Activation Manager\n");

	context_t init_context = act_init(own_context, &init_info, init_base, init_entry, init_tls_base, global_pcc);

	KERNEL_TRACE("kernel", "Going into exception handling mode");

	// We re-use this context as an exception context. Maybe we should create a proper one?
	kernel_exception(init_context, own_context); // Only here can we start taking exceptions, otherwise we crash horribly
	kernel_panic("exception handler should never return");
}
