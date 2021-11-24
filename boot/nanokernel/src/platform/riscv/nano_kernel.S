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
#include "math_utils.h"

.set NANO_SIZE, 0x04000000
.set DEF_DATA_PERMS, (CHERI_PERM_ALL &~ CHERI_PERM_ACCESS_SYS_REGS)
.set IO_DATA_PERMS, (CHERI_PERM_LOAD | CHERI_PERM_STORE)
.set DEF_SEALING_PERMS, (CHERI_PERM_SEAL | CHERI_PERM_UNSEAL | CHERI_PERM_GLOBAL)
.bss

// The alignment required by the object with the highest alignment requirement (likely the phy_page_table)
.set align_require, 18

#########################################################
# nanokernel variables (accessed relative to abi_local) #
#########################################################

START_LOCALS        align_require

local_cap_var       current_context
local_cap_var       next_context

# Most contexts are allocated by the system, but we provide a few statically to bootstrap
local_var           context_table, N_CONTEXTS * CONTEXT_SIZE, CAP_SIZE_BITS

LOCAL_AT    if_table_start
#define IF_ENTRY(name, ...) local_cap_var name ## _table_entry;
NANO_KERNEL_IF_LIST(IF_ENTRY)
LOCAL_AT    if_table_end

local_var           top_virt_table, PAGE_TABLE_SIZE, PHY_PAGE_SIZE_BITS

local_var           tres_bitfield, TRES_BITFIELD_SIZE, 2

# This needs to be aligned enough that the set bounds will work. Number of bits just found by trial and error
local_var           phy_page_table, (TOTAL_PHY_PAGES + 1) * PHY_PAGE_ENTRY_SIZE, align_require
# Pad so the bounds setting in get book works
local_align         align_require

END_LOCALS

########################
# Nano kernel routines #
########################

.text

.macro NANO_FUNC name
    .p2align 2
    .global \name
    \name:
    # TODO: Remove this nop. I just put it in here because most functions were empty.
    nop
.endmacro
.macro NANO_RET
/* MIPS always used cinvoke/ccall as its return operation. This required the ccall2 variant to work well.
 * Instead, we will use sentries that can load a pair. */
    cmove   abi_local, cnull
    cjr     cra
.endm

nano_kernel_start:

diff_table_start:
# This table holds offset from the nano kseg to a particular nanokernel function.
# half word relocations not supported =(
#define DIFF_MACRO(item, ...) .word(item - NANO_KSEG);
#define DIFF_TABLE_ENTRY_SIZE   4
#define DIFF_TABLE_LOAD         clw
NANO_KERNEL_IF_LIST(DIFF_MACRO)
diff_table_end:
.set N_NANO_FUNCS, LIST_LENGTH(NANO_KERNEL_IF_LIST)
.set diff_table_size, DIFF_TABLE_ENTRY_SIZE * N_NANO_FUNCS

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

    # Set up exception vectors
    # Clear registers not used to pass arguments


    # create a capability to our locals
    cspecialr   cs0, ddc      # cs0 will hold a global cap for the rest of init
    lau_relative t0, locals_start


    csetaddr    abi_local, cs0, t0                # abi_local will hold a cap to the vars (_bare_ address)

    cincoffset  cs1, cs0, NANO_KERNEL_TYPE  # cs1 will seal things to be passed to the user

    # Set up the physical book. Use the following convention for a while.
    # ct0: pointer to entry
    # t1: prev
    # t2: length
    # t3: spare

    cinc_bi     ct0, abi_local, phy_page_table, t3
    li          t1, 0

    // ct0 += (length << PHY_PAGE_ENTRY_SIZE_BITS)
    .macro PHY_NEXT
        slli        t3, t2, PHY_PAGE_ENTRY_SIZE_BITS
        cincoffset  ct0, ct0, t3
    .endm
    // ct0.prev = prev
    .macro PHY_LINK_PREV
        csd         t1, PHY_PAGE_OFFSET_prev(ct0)
    .endm
    // prev += length
    .macro PHY_NEXT_PREV
        add         t1, t1, t2
    .endm
    // ct0.state = state
    .macro PHY_SET_STATE state
        li          t3, \state
        csw         t3, PHY_PAGE_OFFSET_status(ct0)
    .endm
    // ct0.len = len
    .macro PHY_LINK_LEN
        csd         t2, PHY_PAGE_OFFSET_len(ct0)
    .endm
    // len = n; ct0.len = len
    .macro PHY_LINK_LEN_N n
        li          t2, \n
        PHY_LINK_LEN
    .endm

    .macro PHY_NEXT2
        PHY_NEXT
        PHY_LINK_PREV
        PHY_NEXT_PREV
    .endm

    # TODO: This is for hardcoded QEMU. In general we might like to read the FDT.
    // IO range
    PHY_SET_STATE   page_io_unused
    PHY_LINK_LEN_N  (IO_SIZE >> PHY_PAGE_SIZE_BITS)
    // Nano owned
    PHY_NEXT2
    PHY_SET_STATE   page_nano_owned
    PHY_LINK_LEN_N  (NANO_SIZE >> PHY_PAGE_SIZE_BITS)
    // System owned
    PHY_NEXT2
    PHY_SET_STATE   page_system_owned
    srli            t2, a0, PHY_PAGE_SIZE_BITS
    PHY_LINK_LEN
    // Free (but dirty)
    PHY_NEXT2
    PHY_SET_STATE   page_dirty
    srli            t2, a0, PHY_PAGE_SIZE_BITS
    li              t3, (PHY_RAM_SIZE - NANO_SIZE) >> PHY_PAGE_SIZE_BITS
    sub             t2, t3, t2
    PHY_LINK_LEN
    // Sentinal
    PHY_NEXT2
    PHY_SET_STATE   page_nano_owned
    PHY_LINK_LEN_N  1

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

    # Set up the nano IF
get_pcc_label:
    auipcc      ct0, 0
    li          t1, PHY_MEM_START_CACHED + NANO_KSEG # diff table holds 16-bit offsets from this
    csetaddr    ct0, ct0, t1
    cllc_rel    ct1, diff_table_start
    cincoffset  ct3, ct1, diff_table_size
    cinc_bi     ct2, abi_local, if_table_start, t5

init_table_loop:
    DIFF_TABLE_LOAD t5, 0(ct1)
    cincoffset  ct4, ct0, t5
    cseal       ct4, ct4, cs1
    csc         ct4, 0(ct2)
    cincoffset  ct2, ct2, CAP_SIZE
    cincoffset  ct1, ct1, DIFF_TABLE_ENTRY_SIZE
    bne         t1, t3, init_table_loop

    # Things passed to the user need to be virtual
    li          t4, PHY_MEM_START_CACHED
    cincoffset  ct5, abi_local, t4
    # We don't set bounds on the sealed data so the nanokernel can derive caps from it
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

    # Create if request
    cincoffset  ct0, cs0, IF_AUTH_TYPE
    li          t1, (1 << N_NANO_FUNCS) - 1
    csetaddr    ct1, cs0, t1
    cseal       ca7, ct1, ct0

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
    # ca7 request token
    # cra (jump target)

    csrr        t0, mstatus
    li          t1, RISCV_S << RISCV_STATUS_MPP_SHIFT
    or          t0, t0, t1
    csrw        mstatus, t0

    # Set exception machine vector
    cllc_rel    ct0, m_exp_vec
    cspecialw   mtcc, ct0

    # TODO set the other exception vectors

    cspecialw    mepcc, cra
    cclear_regs(ra, sp, gp, x4, t0, t1, t2, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, t3, t4, t5, x31)
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

#####################################################################################################
# void get_phy_page(register_t page_n, int cached, register_t npages, cap_pair* out, register_t IO) #
NANO_FUNC get_phy_page
#####################################################################################################

    beqz            a2, get_phy_page_end            # if (npages == 0) goto end
    li              t0, TOTAL_PHY_PAGES
    bge             a0, t0, split_phy_er            # if (page_n >= TOTAL_PHY_PAGES) goto end

    cinc_bi         ct0, abi_local, phy_page_table, t1 # ct0 = book
    slli            t1, a0, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ct0, ct0, t1                    # ct0 = book[pagen]

    // Load constants for atomic
    li              t1, page_io_system              # t1 = state to set to
    li              t2, (1 << page_io_unused) | (1 << page_io_system) # bitmaask of states page can be in
    li              t3, IO_DATA_PERMS               # t3 = perms
    bnez            a4, io                          # if (IO) goto io
    // not_io
    li              t1, page_system_owned
    li              t2, (1 << page_unused) | (1 << page_system_owned)
    li              t3, DEF_DATA_PERMS               # t3 = perms
io:
    li              t5, 1
change_state_retry:
    // go from original state -> t1, if len == a2 && state is in t2
    clr.w.aq        a4, (ct0)                       # a4 = book[page_n].state
    sll             a4, t5, a4
    and             a4, a4, t2
    beqz            a4, get_phy_page_end
    cld             a4, PHY_PAGE_OFFSET_len(ct0)    # a4 = book[page_n].len
    bne             a4, a2, get_phy_page_end
    csc.w.rl        a4, t1, (ct0)
    bnez            a4, change_state_retry

    // state has been changed, make cap both caps
    li              t0, PHY_MEM_START_UNCACHED
    beqz            a1, uncached
    li              t1, (PHY_MEM_START_CACHED - PHY_MEM_START_UNCACHED)
    add             t0, t0, t1
uncached: # t0 = cached/uncached base
    slli            a0, a0, PHY_PAGE_SIZE_BITS
    add             a0, a0, t0                      # a0 = base
    slli            a1, a2, PHY_PAGE_SIZE_BITS      # a1 = length
    auipcc          ca4, 0                          # ca4 is code cap
    csetaddr        ca4, ca4, a0
    csetaddr        ca2, abi_local, a0              # ca2 is data cap
    csetboundsexact ca2, ca2, a1
    csetboundsexact ca4, ca4, a1
    candperm        ca2, ca2, t3
    candperm        ca4, ca4, t3

    csc             ca4, 0(ca3)
    csc             ca2, CAP_SIZE(ca3)
get_phy_page_end:
    cclear_regs(t0)
    NANO_RET

NANO_FUNC create_table

NANO_FUNC create_mapping

NANO_FUNC free_mapping

NANO_FUNC get_top_level_table

NANO_FUNC get_sub_table

NANO_FUNC get_read_only_table

NANO_FUNC make_first_reservation

##########################
# page_t* get_book(void) #
NANO_FUNC get_book
##########################

    # Get and bound capability to the phy book
    cinc_bi         ca0, abi_local, phy_page_table, t0
    # We ensured that there was space after the book for the align
    li              t0, ALIGN_UP_2((TOTAL_PHY_PAGES + 1) * PHY_PAGE_ENTRY_SIZE, align_require)
    csetboundsexact ca0, ca0, t0
    # Set perms properly
    li              t0, CHERI_PERM_LOAD
    candperm        ca0, ca0, t0
    NANO_RET

###################################################################
# void split_phy_page_range(register_t pagen, register_t new_len) #
NANO_FUNC split_phy_page_range
###################################################################

    # pagen in range
    li              t1, TOTAL_PHY_PAGES
    bge             a0, t1, split_phy_er        # if (pagen >= TOTAL_PHY_PAGES) goto er


    cinc_bi         ct0, abi_local, phy_page_table, t1 # ct0 = book
    slli            t1, a0, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ct0, ct0, t1                # ct0 = book[pagen]

    # Set book[pagen].state to page_transaction
    li              t1, page_transaction
    camomaxu.w.aq   t2, t1, (ct0)               # t2 holds old state
    bge             t2, t1, split_phy_er        # if (book[pagen].state >= page_transaction) goto er

    cld             t3, PHY_PAGE_OFFSET_len(ct0) # t3 = book[pagen].len
    bge             a1, t3, split_phy_undo      # if (new_len  >= old_len) goto undo

    # At this point we can commit to the operation

    slli            t4, a1, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ca2, ct0, t4                # ca2 = book[new]
    slli            t4, t3, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ca3, ct0, t4                # ca3 = book[next]

    # set up the new node
    sub             t3, t3, a1                      # t3 = book[pagen].len - new_len
    csw             t2, 0(ca2)                      # book[new].state = state
    csd             t3, PHY_PAGE_OFFSET_len(ca2)    # book[new].len = state
    csd             a0, PHY_PAGE_OFFSET_prev(ca2)   # book[new].prev = pagen

    # point forward
    csd             a1, PHY_PAGE_OFFSET_len(ct0)    # book[pagen].len = new_len
    # point back
    add             t3, a0, a1                      # t3 = new
    csd             a1, PHY_PAGE_OFFSET_prev(ct0)   # book[next].prev = new

split_phy_undo:
    # store back old state
    camoswap.w.rl   zero, t2, (ct0)

split_phy_er:
    cclear_regs(t0, a2, a3)
    NANO_RET


###############################################
# void merge_phy_page_range(register_t pagen) #
NANO_FUNC merge_phy_page_range
###############################################

    # pagen in range
    li              t1, TOTAL_PHY_PAGES
    bge             a0, t1, merge_phy_er        # if (pagen >= TOTAL_PHY_PAGES) goto er

    cinc_bi         ca1, abi_local, phy_page_table, t1 # ca1 = book
    slli            t1, a0, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ca1, ca1, t1                # ca1 = book[pagen]

    # Set book[pagen].state to page_transaction
    li              t0, page_transaction
    camomaxu.w.aq   t1, t0, (ca1)               # t1 = book[pagen].state
    bge             t1, t0, merge_phy_er        # if (book[pagen].state >= page_transaction) goto er
    # look up next entry
    cld             t2, PHY_PAGE_OFFSET_len(ca1) # t2 = book[pagen].len
    beqz            t2, merge_phy_undo
    slli            t3, t2, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ca2, ca1, t3                # ca2 = book[pagen+len]
    # also set to transacting
    camomaxu.w.aq   t3, t0, (ca2)               # t3 = book[pagen+len].state
    bge             t1, t0, merge_phy_undo      # if (book[pagen+len].state >= page_transaction) goto undo
    bne             t3, t1, merge_phy_undo2     # if (states different) goto undo2
    # From here we are comitted
    cld             t3, PHY_PAGE_OFFSET_len(ca2) # t3 = book[pagen+len].len
    slli            t4, t3, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ca3, ca2, t4                # ca3 = book[pagen+len+len2]
    add             t3, t3, t2                  # t3 = len + len2
    # merge lengths
    csd             t3, PHY_PAGE_OFFSET_len(ca1)
    # fixup prev
    csd             a0, PHY_PAGE_OFFSET_prev(ca3)
    # clear fields of middle node
    csd             zero, PHY_PAGE_OFFSET_len(ca2)
    csd             zero, PHY_PAGE_OFFSET_prev(ca2)
    # Set state of middle node, restore state of first node
    li              t3, 0
merge_phy_undo2:
    camoswap.w.rl   zero, t3, (ca2)
merge_phy_undo:
    # store back old state
    camoswap.w.rl   zero, t1, (ca1)
merge_phy_er:
    cclear_regs(a1, a2, a3)
    NANO_RET

#####################################################################
# void zero_page_range(register_t pagen)
NANO_FUNC zero_page_range
#####################################################################

    # pagen in range
    li              t1, TOTAL_PHY_PAGES
    bge             a0, t1, merge_phy_er        # if (pagen >= TOTAL_PHY_PAGES) goto er

    cinc_bi         ca1, abi_local, phy_page_table, t1 # ca1 = book
    slli            t1, a0, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ca1, ca1, t1                # ca1 = book[pagen]

    # Set book[pagen].state to page_transaction (not cleaning)
    li              t0, page_transaction
    camomaxu.w.aq   t1, t0, (ca1)               # t1 = book[pagen].state
    bge             t1, t0, zero_page_er        # if (book[pagen].state >= page_transaction) goto er
    li              t0, page_dirty
    bne             t1, t0, zero_page_undo      # only clean dirty pages

    cld             a2, PHY_PAGE_OFFSET_len(ca1) # a2 = len (in pages)
    slli            a0, a0, PHY_PAGE_SIZE_BITS  # base (bytes)
    slli            a2, a2, PHY_PAGE_SIZE_BITS  # len (bytes)
    li              t0, PHY_MEM_START_CACHED
    add             t0, t0, a0
    csetaddr        ct0, abi_local, t0
    cincoffset      ct1, ct0, a2
    # Zero everything between ct0 and ct1

.set UNROLL_FACTOR,  16                          # How many caps worth of zeros to store per loop
    zero_loop:
.macro store_zero g, i
    csc             cnull, (CAP_SIZE * \i)(ct0)
.endm
    for_range       store_zero, g, 0, UNROLL_FACTOR, 1
    cincoffset      ct0, ct0, (CAP_SIZE * UNROLL_FACTOR)
    bne             t0, t1, zero_loop

    li              t1, page_unused
zero_page_undo:
    camoswap.w.rl   zero, t1, (ca1)             # book[pagen].state = page_unused
zero_page_er:
    cclear_regs(a1, t0, t1)
    NANO_RET

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

###################################
# tres_t res_get(register_t type) #
NANO_FUNC tres_get
###################################

    # Check not a reserved type or too large
    li              t0, NANO_TYPES
    blt             a0, t0, tres_er
    li              t0, TYPE_SPACE
    bge             a0, t0, tres_er

    # Construct the sealing capability (might as well do it now)
    csetaddr        ca0, abi_local, a0
    li              t0, DEF_SEALING_PERMS
    candperm        ca0, ca0, t0
    csetbounds      ca0, ca0, 1
    # and seal it
    li              t0, TRES_TYPE
    csetaddr        ct0, abi_local, t0
    cseal           ca0, ca0, ct0
tres_er:
    cclear_regs(t0)
    NANO_RET

######################################
# sealing_cap tres_take(tres_t tres) #
NANO_FUNC tres_take
######################################
    # A type reservation is just a sealed version of the sealiing capability
    li              t0, TRES_TYPE
    csetaddr        ct0, abi_local, t0
    cunseal         ct0, ca0, ct0               # ct0 is result
    cmove           ca0, cnull

    # Use an atomic or to set the correct bit
    cinc_bi         ca1, abi_local, tres_bitfield, t2
    subi            t3, t0, NANO_TYPES
    li              t2, 1
    andi            t1, t3, 0b11111
    sll             t2, t2, t1                  # t2 is the mask in the word
    srli            t1, t3, 5
    slli            t1, t1, 2                   # t1 is the word the bit is in
    cincoffset      ca1, ca1, t1                # ca1 is a pointer to the word

    camoor.w        t1, t2, (ca1)
    and             t1, t1, t2                  # non-zero if already set
    bnez            t1, tres_take_er

    # otherwise return in ca0
    cmove           ca0, ct0
tres_take_er:
    cclear_regs(t0,a1)
    NANO_RET

NANO_FUNC tres_revoke

NANO_FUNC tres_get_ro_bitfield

NANO_FUNC exception_subscribe

NANO_FUNC exception_return

NANO_FUNC exception_replay

NANO_FUNC exception_signal

NANO_FUNC exception_getcause

NANO_FUNC get_integer_space_cap

    csetaddr    ca0, abi_local, zero
    candperm    ca0, ca0, zero
    NANO_RET

NANO_FUNC modify_hardware_reg

NANO_FUNC interrupts_mask

NANO_FUNC interrupts_soft_set

NANO_FUNC interrupts_get

NANO_FUNC translate_address

##########################################################################
# if_req_auth_t if_req_and_mask(if_req_auth_t req_auth, register_t mask) #
NANO_FUNC if_req_and_mask
##########################################################################

    li          t0, IF_AUTH_TYPE
    csetaddr    abi_local, abi_local, t0
    cunseal     ca0, ca0, abi_local
    and         t0, a0, a1
    csetaddr    ca0, ca0, t0
    cseal       ca0, ca0, abi_local
    NANO_RET

NANO_FUNC nano_dummy


NANO_FUNC m_exp_vec

j           m_exp_vec
