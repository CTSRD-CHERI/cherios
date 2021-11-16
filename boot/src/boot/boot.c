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
#include "plat.h"
#include "uart.h"
#include "crt.h"

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

capability
crt_init_globals_boot()
{
    // This works

    void *gdc = cheri_getdefault();
    void *pcc = cheri_setoffset(cheri_getpcc(), 0);

    uint64_t boot_start;
    uint64_t text_start;
    uint64_t data_start;

    cheri_dla_boot(__boot_segment_start, boot_start);
    cheri_dla_boot(__text_segment_start, text_start);
    cheri_dla_boot(__data_segment_start, data_start);

    __unused capability text_segment = (char*)pcc + text_start;
    __unused capability data_segment = (char*)gdc + data_start;

    capability segment_table[5];

    // These are all set up by the linker script
    segment_table[0] = NULL;
    segment_table[2] = (char*)pcc + boot_start;
    segment_table[3] = (char*)pcc + text_start;
    segment_table[4] = (char*)gdc + data_start;

    // Get something usable
    uint64_t table_start = 0, reloc_start = 0, reloc_end = 0;
    cheri_dla_boot(__cap_table_start, table_start);
    cheri_dla_boot(__start___cap_relocs, reloc_start);
    cheri_dla_boot(__stop___cap_relocs, reloc_end);

    capability cgp = cheri_setoffset(gdc, table_start);
    set_cgp(cgp);

    crt_init_common(segment_table, (struct capreloc *)((char*)gdc + reloc_start),
                    (struct capreloc *)((char*)gdc + reloc_end), RELOC_FLAGS_TLS);

    set_cgp(&__cap_table_start);

    return &__cap_table_local_start;
}

void bootloader_main(capability global_pcc);
void bootloader_main(capability global_pcc) {

	/* Init hardware */
	hw_init();

	/* Initialize elf-loader environment */
	init_elf_loader(global_pcc);

    /* Load the nano kernel. Doing this will install exception vectors */
    boot_printf("Boot: loading nano kernel ...\n");
	nano_init_t * nano_init = (nano_init_t*)cheri_setoffset(global_pcc,load_nano());

    boot_platform_init();

    boot_printf("Boot: loading kernel ...\n");
    size_t entry = load_kernel();

    boot_printf("Boot: loading init ...\n");
    boot_info_t *bi = load_init();

    size_t invalid_length = bi->init_end;
    __unused capability phy_start = cheri_setbounds(cheri_setoffset(cheri_getdefault(), NANO_KSEG), invalid_length);

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

    BOOT_PRINT_CAP(smp_signal_vector);

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
            HW_YIELD;
        }
    }

#endif

    boot_printf("Jumping to nano kernel...\n");
    boot_printf("nanokernel args: kernel mem_size %lx. kernel entry %lx. \n"
                "Args to be passed through to kernel: a0 %lx. a1 %lx. a2 %lx. a3 %lx\n",
                mem_size, entry, args.a0, args.a1, args.a2, args.a3);
    nano_init(mem_size, entry, &args);
}
