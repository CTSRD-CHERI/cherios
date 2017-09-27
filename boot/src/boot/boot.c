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
#include "plat.h"
#include "uart.h"

struct packaged_args {
    register_t a0,a1,a2,a3;
};
typedef void nano_init_t(register_t unmanaged_size, register_t return_ptr, struct packaged_args* args);

#ifdef SMP_ENABLED
enum smp_signal_e {
    spin_wait = 0,
    trampoline_ready = 1,
    trampolined = 2
};

capability smp_destination_vector[SMP_CORES];
volatile char smp_signal_vector[SMP_CORES];
#endif

void bootloader_main(void);
void bootloader_main(void) {

	/* Init hardware */
	hw_init();

	/* Initialize elf-loader environment */
	init_elf_loader();

    /* Load the nano kernel. Doing this will install exception vectors */
    boot_printf("Boot: loading nano kernel ...\n");
	nano_init_t * nano_init = (nano_init_t *)load_nano(); //We have to rederive this as an executable cap
    nano_init = (nano_init_t*)cheri_setoffset(cheri_getpcc(),cheri_getoffset(nano_init));

    /* TODO: we could have some boot exception vectors if we want exception  handling in boot. */
    /* These should be in ROM as a part of the boot image (i.e. make a couple more dedicated sections */
    cp0_status_bev_set(0);

    boot_printf("Boot: loading kernel ...\n");
    size_t entry = load_kernel();

    boot_printf("Boot: loading init ...\n");
    boot_info_t *bi = load_init();

    size_t invalid_length = bi->init_end;
    capability phy_start = cheri_setbounds(cheri_setoffset(cheri_getdefault(), MIPS_KSEG0), invalid_length);

    /* Do we actually need this? */
    //boot_printf("Invalidating %p length %lx:\n", phy_start, invalid_length);
    //caches_invalidate(phy_start, invalid_length);


    register_t mem_size = bi->init_end - bi->nano_end;

    /* Jumps to the nano kernel init. This will completely destroy boot and so we can never return here.
     * All registers will be cleared apart from a specified few. mem_size of memory will be left unmanaged and the
     * rest will be returned as a reservation. The third argument is an extra argument to the kernel */

    BOOT_PRINT_CAP(nano_init);

    struct packaged_args args;
    args.a0 = bi->init_begin - bi->kernel_begin;
    args.a1 = bi->init_entry;
    args.a2 = bi->init_tls_base;
    args.a3 = 0;

#ifdef SMP_ENABLED
    boot_printf("Signalling SMP cores...\n");
    /* We can send each core to a different location - however here will send them all to nano init */
    for(size_t i = 1; i < SMP_CORES; i++) {
        smp_destination_vector[i] = nano_init;
        smp_signal_vector[i] = trampoline_ready;
    }

    /* Wait until all threads have moved on as the bootloader may overwrite itself before a tread moves on -
     * very unlikely, be better safe than sorry */
    boot_printf("Waiting for SMP cores...\n");
    for(size_t i = 1; i < SMP_CORES; i++) {
        while(smp_signal_vector[i] != trampolined) {
            //nop
        }
    }

#endif

    boot_printf("Jumping to nano kernel...\n");

    nano_init(mem_size, entry, &args);
}
