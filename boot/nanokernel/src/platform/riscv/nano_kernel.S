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

local_cap_var       exception_context
local_cap_var       current_context
local_cap_var       next_context    # points to the next context in the context table

local_cap_var       last_exception_victim
local_reg_var       last_exception_cause
local_reg_var       last_exception_stval
local_var           made_first_res, 4, 2

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
/* MIPS always used cinvoke/ccall as its return operation. This required the ccall2 variant to work well.
 * Instead, we will use sentries that can load a pair in most places. To keep the "single domain" caller side
 * simple, we will have the caller provide abi_local in t5. */
.macro NANO_FUNC name
    .p2align 2
    .global \name
    \name:
    # TODO: Remove this nop. I just put it in here because most functions were empty.
.endmacro
.macro NANO_RET
    cmove   abi_local, ct5
    cjr     cra
.endm

.macro NANO_TODO
    TRACE_ON
    1: j 1b
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

// Jumped to if a fault is hit that cannot be recovered.
// Context will need cleaning up, or the system will just be wedged.
nano_kernel_die:
j   nano_kernel_die

// A general error return. Saves having too many error handling cases
nano_er:
cclear_regs(a0,a1,a2,a3,a4,a5,a6,a7,t0,t1,t2,t3,t4)
NANO_RET

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
    cspecialw   stdc, ct5
    # We don't set bounds on the sealed data so the nanokernel can derive caps from it
    add_bi      t4, t4, RAM_START + NANO_SIZE, t1               # vaddr of unmanaged start

    # Make the read-only pointer to the if_table
    cinc_bi     ca4, ct5, if_table_start, t3
    csetbounds  ca4, ca4, (if_table_end - if_table_start)
    li          t3, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP
    candperm    ca4, ca4, t3

    # First context
    cincoffset  ca6, ct5, context_table
    # Set current contexts state to running
    li          t1, CONTEXT_STATE_RUNNING
    csd         t1, (context_table+CONTEXT_OFFSET_STATE)(abi_local)
    li          t0, (CONTEXT_SIZE * N_CONTEXTS)
    csetbounds  ct0, ca6, t0
    csetbounds  ca6, ca6, CONTEXT_SIZE
    csc         ca6, current_context(abi_local)                       # current context is first context
    cincoffset  ct0, ct0, CONTEXT_SIZE
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

    csrrs_bi    zero, mstatus, (RISCV_S << RISCV_STATUS_MPP_SHIFT) | (RISCV_S << RISCV_STATUS_SPP_SHIFT), t0

    # Set exception machine vector
    cllc_rel    ct0, m_exp_vec
    cspecialw   mtcc, ct0

    cllc_rel    ct0, (s_exp_vec)
    cinc_bi     ct0, ct0, PHY_MEM_START_CACHED, t1
    cspecialw   stcc, ct0

    # I have decided, for now, to run all of CheriOS in S-Mode. M-Mode would have been preferable, but MPRV only allows
    # data accesses to be virtualised. On machines with only M-mode and U-Mode, this will be very sadly mean that
    # either virtual memory will not work, or we will have to switch between M-mode and U-mode for many nano calls.

    li          t0, -1
    # Delegate all exceptions / in interrupts to s-mode
    csrw        medeleg, t0
    csrw        mideleg, t0

    cspecialw    mepcc, cra
    cclear_regs(ra, sp, gp, x4, t0, t1, t2, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, t3, t4, t5, x31)
    mret

###################################################################
# context_t create_context(reg_frame_t* initial_state, res_t res) #
NANO_FUNC create_context
###################################################################

    beqz        a1, allocate_static
allocate_dynamic:
        # TODO: the reservation version provides memory dynamically


allocate_static:
    li          t0, CONTEXT_SIZE
    cincoffset  ca2, abi_local, next_context
# atomically increment next_context by CONTEXT_SIZE
1:  clr.c       ca3, (ca2)
    cincoffset  ca4, ca3, t0
    csc.c       t1, ca4, (ca2)
    bnez        t1, 1b
allocated:
    csetboundsexact ca3, ca3, t0

    # memcpy CHERI_FRAME_SIZE bytes from ca0 to ca3
    cmove       ca1, ca3
    add         t1, a0, CHERI_FRAME_SIZE
cpy_loop:
    clc         ca2, 0(ca0)
    csc         ca2, 0(ca1)
    cincoffset  ca0, ca0, CAP_SIZE
    cincoffset  ca1, ca1, CAP_SIZE
    bne         a0, t1, cpy_loop

    # Set magic value to a non-zero value
    li          t0, 1
    csd         t0, CONTEXT_OFFSET_MAGIC(ca3)

    # finally, seal ca3
    li          a1, CONTEXT_TYPE
    csetaddr    ca0, abi_local, a1
    cseal       ca0, ca3, ca0

    cclear_regs(a2, a3, a4)
    NANO_RET

NANO_FUNC destroy_context

######################################################################################
# void context_switch(capability ca0 ... ca7, ct0, ct1, ct2, context_t restore_from) #
NANO_FUNC context_switch
######################################################################################

# This is one of the more complicated functions. It is called both by the system (to voluntarily switch between tasks)
# and as the result of an interrupt/exception. restore_from is the most important argument, and points to a saved context.
# The other arguments are _possibly_ passed through, if the magic value of the target context is 0.
# Note: ct0, ct1, ct2, and restore_from are passed via CSP at present. Once we fix stack passing of arguments,
# they will likely be passed in another register, correctly bounded.
# Note: until they are saved, it is important not to disrupt _any_ registers as this might have been from an exception
# the excpetion to this is abi_local. If called by the system, abi_local will be set by the cinvoke.
# The exception entry should swap with xTDC (which will be saved instead of abi_local)


# Breaking the list of registers into different parts, as some are saved and restored differently
# List 2 and 4 are (possibly) passed through.
# c30/c31 are restored specially

#define rlist1 c1, c2, c3, c4
#define rlist1_start 0

#define rlist2 c5, c6, c7
#define rlist2_start 4

#define rlist3 c8, c9
#define rlist3_start 7

#define rlist4 c10, c11, c12, c13, c14, c15, c16, c17
#define rlist4_start 9

#define rlist5 c18, c19, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29
#define rlist5_start 17

# Note: c30 and c31 are handled specially

    # disable interrupts. t3 will hold old status in case we want to turn them on again
    # TODO: This needs to work at different ELs as well
    csrrci          t3, sstatus, RISCV_STATUS_SIE

    # Write a magic value to cause (currently 0) to indicate a manual switch.
    csrw            scause, zero

context_switch_exception_entry:



#define INDEX_TO_OFFSET(index) ((index) * CAP_SIZE)-INC_IM_MAX

.macro cstore_reg reg, index, g
    csc             \reg, (INDEX_TO_OFFSET(\index))(c30)
.endm

    # Load current context
    clc             c30, current_context(abi_local)
    # Store some registers we always store
    foreachi        cstore_reg, rlist1_start, g, rlist1
    foreachi        cstore_reg, rlist3_start, g, rlist3
    foreachi        cstore_reg, rlist5_start, g, rlist5
    # We put c30 here
    cspecialr       ct0, sscratchc
    csc             ct0, INDEX_TO_OFFSET(30-1)(c30)
    cspecialr       ct1, ddc
    csc             ct1, INDEX_TO_OFFSET(REG_FRAME_INDEX_DEFAULT)(c30)
    csrr            s1, scause
    csd             s1, CONTEXT_OFFSET_MAGIC(c30)
    beqz            s1, stores_called_only
stores_exception_only:
    # These register will not be passed through on next restore, and so need saving
    foreachi        cstore_reg, rlist2_start, g, rlist2
    foreachi        cstore_reg, rlist4_start, g, rlist4
    # TODO This needs to work at different ELs. Not just supervisor.
    # abi_local was swapped with xtdc. Restore tdc at the same time as getting value to store.
    cspecialrw      ct0, stdc, abi_local
    csc             ct0, INDEX_TO_OFFSET(31-1)(c30)
    # restored PCC based on Xepcc
    cspecialr       ct0, sepcc
    csc             ct0, INDEX_TO_OFFSET(REG_FRAME_INDEX_PCC)(c30)
    # Store exception info
    csd             s1, last_exception_cause(abi_local)
    csrr            t0, stval
    csd             t0, last_exception_stval(abi_local)
    li              t0, CONTEXT_TYPE
    csetaddr        ct0, abi_local, t0
    cseal           ct0, c30, ct0
    csc             ct0, last_exception_victim(abi_local)
    # Load context to restore
    clc             cs0, exception_context(abi_local)
    j               context_save_finish
stores_called_only:
    # Caller expects c31 to be restored with c30 (which is currently in ct0)
    csc             ct0, INDEX_TO_OFFSET(31-1)(c30)
    # restored PCC based on ra if this functions is called
    csc             cnull, INDEX_TO_OFFSET(REG_FRAME_INDEX_DEFAULT)(c30)
    csc             cra, INDEX_TO_OFFSET(REG_FRAME_INDEX_PCC)(c30)
    # Unseal context argument
    clc             cs0, (3*CAP_SIZE)(csp)
    li              t0, CONTEXT_TYPE
    csetaddr        ct0, abi_local, t0
    cunseal         cs0, cs0, ct0
    # Load other stack passed arguments
    clc             ct0, 0(csp)             # pass through
    clc             ct1, CAP_SIZE(csp)      # pass through
    clc             ct2, (2*CAP_SIZE)(csp)  # pass through
context_save_finish:
    # NOTE: it is fine to used saved registers, they have all been stored
    cincoffset      cs2, c30, CONTEXT_OFFSET_STATE  # cs2 = pointer to source context state
    li              s3, CONTEXT_STATE_DESTROYED
    camoand.d.rl    s4, s3, (cs2)                       # will set running->created, or destroyed->destroyed
    beq             s3, s4, nano_kernel_die

    # From this point on we are truly out of the save portion and need to restore
context_load: # cs0 = context to load from. s1 = cause of switch (0 if called)
    # Set state of target context to running.
    cmove           c30, cs0                    # more convenient
    cincoffset      cs2, c30, CONTEXT_OFFSET_STATE  # cs2 = pointer to source context state
    li              s3, CONTEXT_STATE_RUNNING
retry:
    camomaxu.d.aq   s4, s3, (cs2)
    beq             s3, s4, retry               # Note: we spin here to handles between multicore scheduling.
    bnez            s4, nano_kernel_die         # Die if changing to a destroyed context
    # and store it as current_context
    csc             c30, current_context(abi_local)

    cld             s3, CONTEXT_OFFSET_MAGIC(c30)
    beqz            s3, target_pass_through     # target accepts pass through
    # Not pass through, so load
.macro cload_reg reg, index, g
    clc             \reg, (INDEX_TO_OFFSET(\index))(c30)
.endm
    foreachi        cload_reg, rlist2_start, g, rlist2
    foreachi        cload_reg, rlist4_start, g, rlist4
    j               source_pass_through
target_pass_through:
    beqz            s1, source_pass_through     # source provided pass through
    # have to zero registers to avoid leaking
    cclear_regs(a0, a1, a2, a3, a4, a5, a6, a7, t0, t1, t2)
source_pass_through:
    foreachi        cload_reg, rlist1_start, g, rlist1
    foreachi        cload_reg, rlist3_start, g, rlist3
    foreachi        cload_reg, rlist5_start, g, rlist5

    # Here we still need to restore c30/c31/ddc/pcc, but can only use c30/c31(abi_local)/ddc/scratch
    # Get ourselves a temp by saving ct0
    cspecialw       sscratchc, ct0
    csrrs_bi        zero, sstatus, (RISCV_S << RISCV_STATUS_SPP_SHIFT), t0

    clc             ct0, INDEX_TO_OFFSET(REG_FRAME_INDEX_DEFAULT)(c30)
    cspecialw       ddc, ct0
    clc             abi_local, INDEX_TO_OFFSET(31-1)(c30)
    clc             ct0, INDEX_TO_OFFSET(REG_FRAME_INDEX_PCC)(c30)
    clc             c30, INDEX_TO_OFFSET((30-1))(c30)
    # currently, scratch holds ct0, ct0 hold pcc
    cspecialw       sepcc, ct0
    cmove           ct0, cnull                      # important to not leak!
    cspecialrw      ct0, sscratchc, ct0
    sret

########################################
# uint8_t critical_section_enter(void) #
NANO_FUNC critical_section_enter
########################################

# TODO

# Should return a hart ID
    li a0, 0
    NANO_RET

####################################
# void critical_section_exit(void) #
NANO_FUNC critical_section_exit
####################################

# TODO
    NANO_RET

####################################################################
# void set_exception_handler(context_t context, register_t cpu_id) #
NANO_FUNC set_exception_handler
####################################################################

# TODO: Multithreading (dont ignore cpu_id)
    # unseal context
    li          t0, CONTEXT_TYPE
    csetaddr    ct0, abi_local, t0
    cunseal     ca0, ca0, ct0

    # store as exception context
    csc         ca0, exception_context(abi_local)
    cclear_regs(a0, t0)
    NANO_RET

NANO_FUNC rescap_take
    NANO_TODO

/*  typedef struct res_nfo_t {size_t length; size_t base;} res_nfo_t;   */
###################################
# res_nfo_t rescap_nfo(res_t res) #
NANO_FUNC rescap_nfo
###################################

    li              t0, RES_TYPE
    csetaddr        ct0, abi_local, t0
    cunseal         ct0, ca0, ct0

    cgetbase        a1, ct0
    cgetoffset      t2, ct0

    addi            a1, a1, RES_META_SIZE
    cld             t1, RES_LENGTH_OFFSET(ct0)
    slli            a0, t1, RES_LENGTH_SHIFT_HIGH
    srli            a0, a0, (RES_LENGTH_SHIFT_HIGH - RES_LENGTH_SHIFT_LOW)
    beqz            t2, normal_info

sub_info:
    # Subfield
    NANO_TODO

normal_info:
    # TODO: sign extend base (but not length)
    cclear_regs     (t0)
    NANO_RET

NANO_FUNC rescap_revoke_start
    NANO_TODO

NANO_FUNC rescap_revoke_finish
    NANO_TODO

####################################################
# res_t  rescap_split(capability res, size_t size) #
NANO_FUNC rescap_split
####################################################
    # check alignment
    andi            t0, a1, (RES_META_SIZE-1)
    bnez            t0, nano_er
    # No splitting subfields
    cgetoffset      t0, ca0
    bnez            t0, nano_er
    # Unseal
    li              t0, RES_TYPE
    csetaddr        ca3, abi_local, t0
    cunseal         ca0, ca0, ca3
    # New length field
    srl             t4, a1, RES_LENGTH_SHIFT_LOW
    # WARN: _MUST_ materialise the new cap before the store conditional to avoid revocation race.
    cincoffset      ca2, ca0, a1
    cincoffset      ca2, ca2, RES_META_SIZE
    # Rematerialise from abi_local to get new bounds
    csetaddr        ca2, abi_local, a2
    csetbounds      ca2, ca2, RES_META_SIZE
split_loop:
    # t0 = encoded state + length
    clr.d.aq        t0, (ca0)
    # t1 = encoded state
    srli            t1, t0, (RES_STATE_IN_LENGTH_SHIFT + RES_SIMPLE_SHIFT)
    # t2 = state
    andi            t2, t1, (RES_STATE_MASK >> RES_SIMPLE_SHIFT)
    # State must be open
    bnez            t2, nano_er
    # Store high terminator for later
    andi            t2, t1, RES_HIGH_TERM_MASK >> RES_SIMPLE_SHIFT
    # Extract length
    sll             t3, t0, RES_LENGTH_SHIFT_HIGH
    srl             t3, t3, RES_LENGTH_SHIFT_HIGH
    # Must have _more_ length than request size (because also require space for metadata)
    bgeu            t4, t3, nano_er
    # New state (keep low and state, and or in new length)
    andi            t1, t1, (RES_LOW_TERM_MASK >> RES_SIMPLE_SHIFT)
    slli            t1, t1, RES_STATE_IN_LENGTH_SHIFT + RES_SIMPLE_SHIFT
    or              t1, t1, t4
    csc.d.rl        t1, t1, (ca0)
    bnez            t1, split_loop

    # Now create new node. ca2 holding ptr.
    sub             t3, t3, t4
    subi            t3, t3, (RES_META_SIZE >> RES_LENGTH_SHIFT_LOW)
    slli            t2, t2, RES_STATE_IN_LENGTH_SHIFT + RES_SIMPLE_SHIFT
    or              t3, t3, t2
    camoswap.d.rl   zero, t3, 0(ca2)

    cseal           ca0, ca2, ca3

    cclear_regs     (a2, a3)
    NANO_TODO

NANO_FUNC rescap_merge
    NANO_TODO

NANO_FUNC rescap_parent
    NANO_TODO

NANO_FUNC rescap_splitsub
    NANO_TODO

NANO_FUNC rescap_getsub
    NANO_TODO

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
    li              a5, 1
change_state_retry:
    // go from original state -> t1, if len == a2 && state is in t2
    clr.w.aq        a4, (ct0)                       # a4 = book[page_n].state
    sll             a4, a5, a4
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

###############################################################################
# ptable_t create_table(register_t page_n, ptable_t parent, register_t index) #
NANO_FUNC create_table
###############################################################################
    li              a4, 0 # a4 will be the result
    # Only allowed ptable_t type values are VTABLE_TYPE_L0 and VTABLE_TYPE_L0+1
    cgettype        t0, ca1
    li              t1, VTABLE_TYPE_L0
    beq             t0, t1, correct_type
    add             t1, t1, 1
    bne             t0, t1, create_table_error
correct_type:
    # unseal table
    csetaddr        ca3, abi_local, t0
    cunseal         ca1, ca1, ca3

    # !(index < PAGE_TABLE_ENT_PER_TABLE) goto error
    sltiu           t0, a2, PAGE_TABLE_ENT_PER_TABLE
    beqz            t0, create_table_error

    # (page_n >= TOTAL_PHY_PAGEs) goto error
    li              t0, TOTAL_PHY_PAGES
    bgeu            a0, t0, create_table_error

    cinc_bi         ct0, abi_local, phy_page_table, t1
    slli            t1, a0, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ct0, ct0, t1                            # ct0 = &book[page_n]

    # ca1 = parent vtable entry address
    slli            a2, a2, PAGE_TABLE_ENT_BITS
    cincoffset      ca1, ca1, a2

    # a2 = new table entry
    slli            a2, a0, RISCV_PTE_PFN_SHIFT
    # TODO: XWR = 0 means table entry. Do other bits matter? i.e., do we to pay attention to user/global/dirty bits?
    ori             a2, a2, RISCV_PTE_V

    # Create a capability to return to the caller for the new table now
    sll             t1, a0, PHY_PAGE_SIZE_BITS
    add_bi          t1, t1, PHY_MEM_START_CACHED, t2
    csetaddr        ca0, abi_local, t1                      # ca0 = new table

    li              t1, page_transaction
    camomaxu.w.aq   t2, t1, (ct0)               # t2 holds old state

    # allowed states are page_ptable_free and page_unused(0)
    beqz            t2, create_table_phys_state_good
    li              t3, page_ptable_free
    bne             t2, t3,  create_table_undo

    # Because this table has been used before, its a good idea to zero it.
    # If this leaves any physical pages in an unrecoverable state, it is the system's problem
    cmove           ca6, ca0
    cinc_bi         ca7, ca6, PHY_PAGE_SIZE, t3
    # do_zero_page: ca5 return address. ca6 start. ca7 end. clobbers these registers.
    cjal            ca5, do_zero_page

create_table_phys_state_good:
    # length must be 1
    cld             t1, PHY_PAGE_OFFSET_len(ct0)
    li              t3, 1
    bne             t1, t3, create_table_undo

    # compare and swap, expecting zero, with the entry
    1:  clr.d.aq    t1, (ca1)
    bnez            t1, create_table_undo
    csc.d.rl        t1, a2, (ca1)
    bnez            t1, 1b

    # bound and seal ca0. ca3 still has a sealing cap with the parent type
    csetbounds_bi   ca0, ca0, PAGE_TABLE_SIZE, t1
    cincoffset      ca3, ca3, 1
    cseal           ca4, ca0, ca3
    # finally, set physical page to page table state
    li              t2, page_ptable
create_table_undo:
    # store back old state
    camoswap.w.rl   zero, t2, (ct0)
create_table_error:
    cmove           ca0, ca4
    cclear_regs     (a1, a3, a5, a6, a7, t0)
    NANO_RET

##################################################################################
# void create_mapping(register_t page_n, ptable_t table, register_t index_start, #
#                     register_t index_stop, register_t flags)                   #
NANO_FUNC create_mapping
##################################################################################
    # For now, only allowing leaf entries to be at L2
    # Unseal table
    li              t0, VTABLE_TYPE_L2
    csetaddr        ct0, abi_local, t0
    cunseal         ca1, ca1, ct0

    # Do not allow 0 length ranges
    beq             a2, a3, create_mapping_er
    sltiu           t0, a2, PAGE_TABLE_ENT_PER_TABLE
    beqz            t0, create_mapping_er               # index_start < PAGE_TABLE_ENT_PER_TABLE
    sltiu           t0, a3, PAGE_TABLE_ENT_PER_TABLE+1  # index_stop < PAGE_TABLE_ENT_PER_TABLE + 1

    # (page_n >= TOTAL_PHY_PAGEs) goto error
    li              t0, TOTAL_PHY_PAGES
    bgeu            a0, t0, create_mapping_er

    cinc_bi         ct0, abi_local, phy_page_table, t1
    slli            t1, a0, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset      ct0, ct0, t1                            # ct0 = &book[page_n]

    # Set physical page to transacting
    li              t1, page_transaction
    camomaxu.w.aq   t2, t1, (ct0)                           # t2 holds old state

    # State must be free (0)
    bnez            t2, create_mapping_undo

    # Length must be equal to difference in indexs
    cld             t1, PHY_PAGE_OFFSET_len(ct0)            # length
    sub             t3, a3, a2
    bne             t3, t1, create_mapping_undo

    # Mask out flags that the system should not control
#define PTE_FLAGS_SYSTEM (RISCV_PTE_PERM_ALL &~ RISCV_PTE_V)
    and_bi          a4, a4, PTE_FLAGS_SYSTEM, t3

    # a0 = entry
    slli            a0, a0, RISCV_PTE_PFN_SHIFT
    or              a0, a0, a4
    ori             a0, a0, RISCV_PTE_V

    # Loop from     ca1[a2] to ca1[a3], setting entries to a0. And adding (1 << RISCV_PTE_PFN_SHIFT) to a0 each time.
    # If at any point we find a bad entry, we do not undo, instead we keep the mappings we have already
    # made but mark the physical pages as forever invalid
    li              t2, page_screwed_the_pooch

    slli            a2, a2, PAGE_TABLE_ENT_BITS
    slli            a3, a3, PAGE_TABLE_ENT_BITS
    cincoffset      ca2, ca1, a2
    cincoffset      ca3, ca1, a3
    li              a1, (1 << RISCV_PTE_PFN_SHIFT)
set_mapping_loop:
    # compare and swap, expecting zero, with the entry
    1:  clr.d.aq    t1, (ca2)
    bnez            t1, create_mapping_undo
    csc.d.rl        t1, a0, (ca2)
    bnez            t1, 1b
    # Increment page, increment pointer to entry
    add             a0, a0, a1
    cincoffset      ca2, ca2, PAGE_TABLE_ENT_SIZE
    bne             a2, a3, set_mapping_loop

    # All mappings created, set physical pages to mapped
    li              t2, page_mapped
create_mapping_undo:
    # store back old state
    camoswap.w.rl   zero, t2, (ct0)
create_mapping_er:
    cclear_regs     (a2, a3, t0)
    NANO_RET

NANO_FUNC free_mapping
    # It should be noted there are now some entries in the L0 table that cannot be unmapped.
    # Right now, it will impossible to free them because we are not suporting gigapages for the system.
    # When we do, this is will have to do an extra check.
    NANO_TODO

######################################
# ptable_t get_top_level_table(void) #
NANO_FUNC get_top_level_table
######################################
    cinc_bi         ca0, abi_local, top_virt_table, t0
    csetbounds_bi   ca0, ca0, PAGE_TABLE_SIZE, t0
    li              t0, VTABLE_TYPE_L0
    csetaddr        ca1, abi_local, t0
    cseal           ca0, ca0, ca1
    cclear_regs     (a1)
    NANO_RET

############################################################
# ptable_t get_sub_table(ptable_t table, register_t index) #
NANO_FUNC get_sub_table
############################################################
    NANO_TODO

#########################################################
# readable_table_t* get_read_only_table(ptable_t table) #
NANO_FUNC get_read_only_table
#########################################################
    NANO_TODO

#######################################
# res_t make_first_reservation (void) #
NANO_FUNC make_first_reservation
#######################################
.set VIRT_MEM_START,                0x0000  # Mips started at a page so as to never use address 0
.set VIRT_MEM_END,                  (MAX_VIRTUAL_PAGES << UNTRANSLATED_BITS) - ((GIGA_MAPPINGS << 30) * 2)
.set VIRT_MEM_SIZE,                 VIRT_MEM_END - VIRT_MEM_START
    # Set made_first_res 0->1 atomically
    li                  t1, 1
    cincoffset          ct0, abi_local, made_first_res
    camomaxu.w.aqrl     t1, t1, (ct0)
    bnez                t1, nano_er

    # Make a cap to the start of vmem
    li                  t0, VIRT_MEM_START
    csetaddr            ca0, abi_local, t0

    # Open reservation With low terminator And high terminator covering all of vmem minus metadata
    li                  t0, ((RES_LOW_TERM_MASK | RES_HIGH_TERM_MASK) << RES_STATE_IN_LENGTH_SHIFT) | ((VIRT_MEM_SIZE - RES_META_SIZE) >> RES_LENGTH_SHIFT_LOW)
    csd                 t0, RES_LENGTH_OFFSET(ca0)

    li                  t0, RES_TYPE
    csetaddr            ct0, abi_local, t0
    # We must set bounds because we use the offset field to encode extra information
    csetbounds          ca0, ca0, RES_META_SIZE
    cseal               ca0, ca0, ct0
    NANO_RET

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
    csd             t3, PHY_PAGE_OFFSET_prev(ca3)   # book[next].prev = new

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
    csetaddr        ca6, abi_local, t0
    cincoffset      ca7, ca6, a2
    cjal            ca5, do_zero_page

    li              t1, page_unused
zero_page_undo:
    camoswap.w.rl   zero, t1, (ca1)             # book[pagen].state = page_unused
zero_page_er:
    cclear_regs(a1, a6, t1)
    NANO_RET

    # Zero everything between ca6 and ca7. If ca5 is zero, fall through, otherwise return to it.
    # Only clobber these registers
do_zero_page:
.set UNROLL_FACTOR,  16                          # How many caps worth of zeros to store per loop
    zero_loop:
.macro store_zero g, i
    csc             cnull, (CAP_SIZE * \i)(ca6)
.endm
    for_range       store_zero, g, 0, UNROLL_FACTOR, 1
    cincoffset      ca6, ca6, (CAP_SIZE * UNROLL_FACTOR)
    bne             a6, a7, zero_loop
    cjr             ca5

####################################################
# capability obtain_super_powers(capability*, pcc) #
NANO_FUNC obtain_super_powers
####################################################

    auipcc          ct0, 0
    csetaddr        ct1, ct0, zero
    csc             ct1, 0(ca0)
    csetaddr        cra, ct0, ra
    csetaddr        ca0, abi_local, zero
    NANO_RET

/*
typedef struct {
    context_t victim_context;
    register_t cause;
    register_t stval;
    register_t ex_level;
} exection_cause_t;
*/
##################################################
# void get_last_exception(exection_cause_t* out) #
NANO_FUNC get_last_exception
##################################################
    clc             ct0, last_exception_victim(abi_local)
    cld             t1, last_exception_cause(abi_local)
    cld             t2, last_exception_stval(abi_local)
    csc             ct0, 0(ca0)
    csd             t1, CAP_SIZE(ca0)
    csd             t2, (CAP_SIZE + REG_SIZE)(ca0)
    NANO_RET

NANO_FUNC foundation_create
    NANO_TODO

NANO_FUNC foundation_enter
    NANO_TODO

NANO_FUNC foundation_entry_expose
    NANO_TODO

NANO_FUNC foundation_entry_get_id
    NANO_TODO

NANO_FUNC make_key_for_auth
    NANO_TODO

NANO_FUNC foundation_entry_vaddr
    NANO_TODO

NANO_FUNC foundation_new_entry
    NANO_TODO

NANO_FUNC rescap_take_authed
    NANO_TODO

NANO_FUNC rescap_check_cert
    NANO_TODO

NANO_FUNC rescap_check_single_cert
    NANO_TODO

NANO_FUNC rescap_take_locked
    NANO_TODO

NANO_FUNC rescap_unlock
    NANO_TODO

NANO_FUNC foundation_get_id
    NANO_TODO

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
    NANO_TODO

NANO_FUNC tres_get_ro_bitfield
    NANO_TODO

NANO_FUNC exception_subscribe
    # TODO: this should register for user exceptions
    NANO_RET

NANO_FUNC exception_return
    NANO_TODO

NANO_FUNC exception_replay
    NANO_TODO

NANO_FUNC exception_signal
    NANO_TODO

NANO_FUNC exception_getcause
    NANO_TODO

NANO_FUNC get_integer_space_cap

    csetaddr    ca0, abi_local, zero
    candperm    ca0, ca0, zero
    NANO_RET

NANO_FUNC modify_hardware_reg
    NANO_TODO

NANO_FUNC interrupts_mask
    NANO_TODO

NANO_FUNC interrupts_soft_set
    NANO_TODO

NANO_FUNC interrupts_get
    NANO_TODO

NANO_FUNC translate_address
    NANO_TODO

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
NANO_RET

.macro nano_s_ret
    cspecialrw  abi_local, stdc, abi_local
    cspecialrw  c30, sscratchc, c30
    sret
.endm

# This function is called via a syscall so it needs to not clobber registers and has a weird ABI.
# a0 = 0 (used to indicate this is an intercepted call, can be used as a temp)
# a1 = method nb
# ca2 = auth token
# ca3 = result coda
# ca4 = result data
# abi_local = capability to local vars (will be swapped back with stdc on return, do not clobber)
get_if:
    # cause a trap if auth token not tagged/sealed correctly
    li          a0, IF_AUTH_TYPE
    csetaddr    ca0, abi_local, a0
    cunseal     ca0, ca2, ca0

    # null result by default

    cmove       ca3, cnull
    cmove       ca4, cnull

    srl         a0, a2, a1
    andi        a0, a0, 1               # 1 if method allowed
    beqz        a0, exp_skip_inst

    # Seal result data
    li          a0, NANO_KERNEL_TYPE
    csetaddr    ca0, abi_local, a0
    cseal       ca4, abi_local, ca0
    # load method
    sll         a0, a1, CAP_SIZE_BITS
    add_bi      a0, a0, if_table_start, a3
    cincoffset  ca0, abi_local, a0
    clc         ca3, 0(ca0)

    move        a0, zero                # this started zero, it shall end zero

    exp_skip_inst:
    cspecialrw  ct0, sepcc, ct0
    csrw        sscratch, t1
    clw         t1, 0(ct0)
    # Assuming nothing bigger than 32-bit in use. 0b11 is 32bit. Otherwise is 16-bit
    andi        t1, t1, 0b11
    addi        t1, t1, 1
    srli        t1, t1, 1
    andi        t1, t1, 0b10
    addi        t1, t1, 2
    cincoffset  ct0, ct0, t1
    csrr        t1, sscratch
    cspecialrw  ct0, sepcc, ct0
    nano_s_ret

.p2align 2
s_exp_vec:
# This exception vector is the one we normally expect to fall into
    cspecialrw  abi_local, stdc, abi_local
    # Switch with a scratch register. Now abi_local AND c30 are available.
    # Its worth noting that abi_local is c31. This means c30 and c31 (the last things to save) are stored differently
    cspecialrw  c30, sscratchc, c30
    csrr        x30, scause
    subi        x30, x30, RISCV_CAUSE_CALL_S
    bnez        x30, context_switch_exception_entry
    beqz        a0, get_if
    j           context_switch_exception_entry
# TODO intercept user handled exceptions

# Machine mode exception vector
m_exp_vec:
    j           nano_kernel_die
