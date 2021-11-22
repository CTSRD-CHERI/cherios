/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lawrence Esswood
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include "asm.S"
#include "nano/nano_if_list.h"
#include "nano/nanotypes.h"
#include "cheric.h"
#include "reg_abi.h"

.set NANO_SIZE, 0x04000000
.set DEF_DATA_PERMS, (CHERI_PERM_ALL &~ CHERI_PERM_ACCESS_SYS_REGS)

.bss

#########################################################
# nanokernel variables (accessed relative to abi_local) #
#########################################################

START_LOCALS        PHY_PAGE_SIZE_BITS

local_cap_var       current_context
local_cap_var       next_context

# Most contexts are allocated by the system, but we provide a few statically to bootstrap
local_var           context_table, N_CONTEXTS * CONTEXT_SIZE, CAP_SIZE_BITS

LOCAL_AT    if_table_start
#define IF_ENTRY(name, ...) local_cap_var name;
NANO_KERNEL_IF_LIST(IF_ENTRY)
LOCAL_AT    if_table_end

local_var           top_virt_table, PAGE_TABLE_SIZE, PHY_PAGE_SIZE_BITS

END_LOCALS

########################
# Nano kernel routines #
########################

.text

.macro NANO_FUNC name
    .p2align 2
    .global \name
    \name:
    nop
.endmacro

nano_kernel_start:

diff_table_start:
# This table holds offset from the nano kseg to a particular nanokernel function.
# The - NANO_KSEG not needed due to truncation
#define DIFF_MACRO(item, ...) .2byte (item);
NANO_KERNEL_IF_LIST(DIFF_MACRO)
diff_table_end:
.set diff_table_size, 2 * LIST_LENGTH(NANO_KERNEL_IF_LIST)

# The nanokernel will use a number of gigapages to create a physical memory map
# One cached and one uncached should do.

# A bit wasteful, but I am just going to steal 1/4 of the the (virtual) address space.
# I could just steal 2 * ((TOTAL_PHY_PAGES << PHY_PAGE_SIZE_BITS) >> 30).
# might change to that later, but this will make things simple for now.


#define GIGA_MAPPINGS               (0x1000000000 >> 30)

// These should stay contiguous
#define PHY_MEM_START_CACHED        0xfffffff000000000
#define PHY_MEM_START_UNCACHED      0xffffffe000000000

// On real hardware we may actually need a second range

######################################################################################
# nano_kernel_init(register_t unmanaged_space, register_t entry_offset, packaged args)
NANO_FUNC nano_kernel_init
######################################################################################

# The bootloader will have already layed out this nanokernel and some 'unmanaged_space'
# For CheriOS, the unmanaged space contains the kernel and init process
# This init runs in m-mode, and returns to s-mode/h-mode (if supported)

# In the future, we might get the unamanged_space to be a slightly modified bbl.
# in which case, we would stay in m-mode.

# TODO RISCV
# Things still to do in init:

    # Lay out the physical page table
    # Set up exception vectors
    # Clear registers not used to pass arguments


    # create a capability to our locals
    cspecialr   cs0, ddc      # cs0 will hold a global cap for the rest of init
    lau_relative t0, locals_start


    csetaddr    abi_local, cs0, t0                # abi_local will hold a cap to the vars (_bare_ address)

    cincoffset  cs1, cs0, NANO_KERNEL_TYPE  # cs1 will seal things to be passed to the user

    # Set up the MMU
    # First, create gigapage mappings

    # A pointer to the first PTE to set
    li          t0, top_virt_table +                                                    \
                       ((PHY_MEM_START_UNCACHED & ((1 << 39) - 1)) >>                                       \
                       (L2_BITS + L1_BITS + PHY_PAGE_SIZE_BITS - PAGE_TABLE_ENT_BITS))
    cincoffset  ct0, abi_local, t0
    li          t3, 1   # 1 if uncached. 0 if cached. currently ignored because QEMU
    li          t4, (1 << ((30 - PHY_PAGE_SIZE_BITS) + RISCV_PTE_PFN_SHIFT))    # increment in gigabytes
    set_giga_restart:
    # A pointer to just past the end of the last PTE to set
    cincoffset  ct1, ct0, (GIGA_MAPPINGS << PAGE_TABLE_ENT_BITS)

    # The PTE
    li          t2, (RISCV_PTE_PERM_ALL)
    set_giga_start:
    csd         t2, 0(ct0)
    cincoffset  ct0, ct0, PAGE_TABLE_ENT_SIZE
    add         t2, t2, t4
    bne         t0, t1, set_giga_start

    # again for uncached mappings (which will just be the same for now)
    cmove       ct0, ct1
    subi        t3, t3, 1
    beqz        t3, set_giga_restart

    # Point satp correctly
    srli        t0, x31, PHY_PAGE_SIZE_BITS
    add_bi      t0, t0, (top_virt_table >> PHY_PAGE_SIZE_BITS) | (RISCV_SATP_MODE_Sv39 << RISCV_SATP_MODE_SHIFT), t1
    csrw        satp, t0
    sfence.vma

    TRACE_OFF
    # Set up the nano IF
    get_pcc_label:
    cllc_rel    ct0, NANO_KSEG                  # diff table holds 16-bit offsets from this
    cllc_rel    ct1, diff_table_start
    cincoffset  ct3, ct1, diff_table_size
    cinc_bi     ct2, abi_local, if_table_start, t5

    init_table_loop:
    clh         t5, 0(ct1)
    cincoffset  ct4, ct0, t5
    cseal       ct4, ct4, cs1
    csc         ct4, 0(ct2)
    cincoffset  ct2, ct2, CAP_SIZE
    cincoffset  ct1, ct1, 2
    bne         t1, t3, init_table_loop

    # Things passed to the user need to be virtual
    li          t4, PHY_MEM_START_CACHED
    cincoffset  ct5, abi_local, t4
    li          t1, locals_size
    csetbounds  ct5, ct5, t1
    add_bi      t4, t4, RAM_START + NANO_SIZE, t1               # vaddr of unmanaged start

    # Make the read-only pointer to the if_table
    cinc_bi     ca4, ct5, if_table_start, t3
    csetbounds  ca4, ca4, (if_table_end - if_table_start)
    li          t3, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP
    candperm    ca4, ca4, t3

    # First context
    cincoffset  ca6, ct5, context_table
    csc         ca6, current_context(abi_local)                       # current context is first context
    cincoffset  ct0, ca6, CONTEXT_SIZE
    csc         ct0, next_context(abi_local)                          # store pointer to next context to allocate
    # Seal current context
    cincoffset  ct0, cs0, CONTEXT_TYPE
    cseal       ca6, ca6, ct0

    # And also seal data argument
    cseal       ca5, ct5, cs1

    # Correctly bounds PCC (to jump to)
    cspecialr   cra, pcc
    csetaddr    cra, cra, t4
    csetbounds  cra, cra, a0
    cincoffset  cra, cra, a1
    li          t0, DEF_DATA_PERMS
    candperm    cra, cra, t0

    # correctly bound DDC
    csetaddr    ct1, cs0, t4
    csetbounds  ct1, ct1, a0
    candperm    ct1, ct1, t0
    cspecialw   ddc, ct1

    # Pass through arguments to the kernel
    cmove       ct0, ca2
    cld         a0, 0x0(ct0)
    cld         a1, 0x8(ct0)
    cld         a2, 0x10(ct0)
    cld         a3, 0x18(ct0)

    # Not clearing:
    # default data (but it only covers the unmanaged space)
    # a0 through a3 (passed from boot)
    # ca4 if_table
    # ca5 sealed data
    # ca6 first context handle
    # ca7 request token (TODO)
    # cra (jump target)

    TRACE_ON
    csrr        t0, mstatus
    li          t1, RISCV_S << RISCV_STATUS_MPP_SHIFT
    or          t0, t0, t1
    csrw        mstatus, t0
    csrr        t0, mstatus
    csrr        t0, satp
    # Set exception vector
    cllc_rel    ct0, m_exp_vec
    cspecialw   mtcc, ct0

    # TODO: clear all the registers
    cspecialw    mepcc, cra
    mret

NANO_FUNC create_context

NANO_FUNC destroy_context

NANO_FUNC context_switch

NANO_FUNC critical_section_enter

NANO_FUNC critical_section_exit

NANO_FUNC set_exception_handler

NANO_FUNC rescap_take

NANO_FUNC rescap_nfo

NANO_FUNC rescap_revoke_start

NANO_FUNC rescap_revoke_finish

NANO_FUNC rescap_split

NANO_FUNC rescap_merge

NANO_FUNC rescap_parent

NANO_FUNC rescap_splitsub

NANO_FUNC rescap_getsub

NANO_FUNC get_phy_page

NANO_FUNC create_table

NANO_FUNC create_mapping

NANO_FUNC free_mapping

NANO_FUNC get_top_level_table

NANO_FUNC get_sub_table

NANO_FUNC get_read_only_table

NANO_FUNC make_first_reservation

NANO_FUNC get_book

NANO_FUNC split_phy_page_range

NANO_FUNC merge_phy_page_range

NANO_FUNC zero_page_range

NANO_FUNC obtain_super_powers

NANO_FUNC get_last_exception

NANO_FUNC foundation_create

NANO_FUNC foundation_enter

NANO_FUNC foundation_entry_expose

NANO_FUNC foundation_entry_get_id

NANO_FUNC make_key_for_auth

NANO_FUNC foundation_entry_vaddr

NANO_FUNC foundation_new_entry

NANO_FUNC rescap_take_authed

NANO_FUNC rescap_check_cert

NANO_FUNC rescap_check_single_cert

NANO_FUNC rescap_take_locked

NANO_FUNC rescap_unlock

NANO_FUNC foundation_get_id

NANO_FUNC tres_get

NANO_FUNC tres_take

NANO_FUNC tres_revoke

NANO_FUNC tres_get_ro_bitfield

NANO_FUNC exception_subscribe

NANO_FUNC exception_return

NANO_FUNC exception_replay

NANO_FUNC exception_signal

NANO_FUNC exception_getcause

NANO_FUNC get_integer_space_cap

NANO_FUNC modify_hardware_reg

NANO_FUNC interrupts_mask

NANO_FUNC interrupts_soft_set

NANO_FUNC interrupts_get

NANO_FUNC translate_address

NANO_FUNC if_req_and_mask

NANO_FUNC nano_dummy


NANO_FUNC m_exp_vec

j           m_exp_vec
