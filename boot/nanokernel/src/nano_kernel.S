# -
# Copyright (c) 2017 Lawrence Esswood
# All rights reserved.
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
# ("CTSRD"), as part of the DARPA CRASH research programme.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#define __ASSEMBLY__ 1
.set MIPS_SZCAP, _MIPS_SZCAP
.include "asm.S"
#include "assembly_utils.h"
#include "nano/nanokernel.h"

# We will lay out our contexts like this

# struct context_t {
#   reg_frame_t
#   enum state{allocated, dead} (size of a cap)
#   found_id_t* foundation
#};

.set CONTEXT_SIZE, CHERI_FRAME_SIZE + CAP_SIZE + CAP_SIZE


.set N_CONTEXTS,         64             # We never reallocate these, so its currently a huge limitation

#struct res_t {
#    enum state{open, taken, merged, revoking}
#    size_t length <- length of reservation (not including meta data). Not set when open.
#    size_t pid <- an id for the parent node. Checked for merging
#    padding to size of one cap
#    user data to pad to size of cache_line
#    union{cap for region, user data} <- store a cheeky cap here when open for ease. Part of user data o/w
#
#}

.set CACHE_LINE_SIZE,                64

# Top bits of virtual address decide which range we are using
.set PHY_MEM_START_TOP,             0x90
.set PHY_MEM_START_UNCACHED_TOP,    0x98
.set VIRT_MEM_START_TOP,            0x00
.set VIRT_SUPER_TOP,                0x40
.set VIRT_KERN_TOP,                 0xC0

# leaving 56 bits free
.set TOP_ADDR_SHIFT,                56
.set TOP_ADDR_BITS,                 8


.set VIRT_MEM_START,                0x2000
.set VIRT_MEM_END,                  PHY_MEM_START_TOP << TOP_ADDR_SHIFT # virt mem ends where phy begins
.set VIRT_MEM_SIZE,                 VIRT_MEM_END - VIRT_MEM_START

# This is the physical memory we will scan in order to support revocation
.set PHY_MEM_START,                 PHY_MEM_START_TOP << TOP_ADDR_SHIFT
.set PHY_MEM_START_UNCACHED,        PHY_MEM_START_UNCACHED_TOP << TOP_ADDR_SHIFT

.set PHY_MEM_END,                   PHY_MEM_START + PHY_MEM_SIZE

#.set NANO_SIZE, __nano_size
.set NANO_SIZE, 0x5000000

# We will lay out PHYSICAL state like this. The PFN is just the index in the table:
# struct phy_ent {
#    register_t state           {un-used, nano_owned, system_owned, mapped}
#    register_t len             number of pages in this chunk
#    register_t prev            index to prev
#    register_t next            index to next
#
#};


# The general idea is this: Physical pages are un-used and their state can be set (once) by the system
# Some will be set to nano_owned for private use (constant set at init time).
# Some will system_owned, we hand out PHYSICAL capabilities
# Some will be mapped, and we will remember the mapping. When the VPN is proveably not in use, we can go back to un-used

####################################################################################################################
# The (very incorrectly named) global data for the nano kernel. idc will hold a capability that covers the range
# locals_start to locals_end.
####################################################################################################################

# The small locals can be accessed from idc with a constant. The others need dli. The address you get is relative
# to idc. If you want a global address to use name_label.

START_LOCALS CAP_SIZE_BITS

# Stuff to do with context switch #
local_cap_var current_context
local_cap_var exception_context
local_cap_var next_context
local_cap_var victim_context
local_reg_var exception_level
local_reg_var exception_cause
local_reg_var exception_ccause
local_reg_var last_exception_cause
local_reg_var last_exception_ccause
local_reg_var last_bad_vaddr

# Stuff to do with virtual space management
local_reg_var made_first_res
local_reg_var revoke_state
local_reg_var revoke_base
local_reg_var revoke_bound

# Method table for nano kernel
NANO_KERNEL_IF_LIST(LOCAL_CAP_VAR_MACRO)
.set cap_table_end, local_ctr

local_var top_virt_page, PAGE_TABLE_SIZE, PAGE_TABLE_ENT_BITS

local_var context_table, N_CONTEXTS * CONTEXT_SIZE, CAP_SIZE_BITS

local_var phy_page_table, (TOTAL_PHY_PAGES + 1) * PHY_PAGE_ENTRY_SIZE, PHY_PAGE_ENTRY_SIZE_BITS

END_LOCALS

.text
.section .init


.set DEF_DATA_PERMS, (Perm_All & ~(Perm_Access_System_Registers | Perm_Seal))
.set DEF_SEALING_PERMS, Perm_Seal

nano_kernel_start:

# We have no stack, and this sub routine may need to be callable from a couple of places. Set appropriately.

.set zero_page_tmp_1,   $t8
.set zero_page_ctmp_1,  $c13
.set zero_page_arg,     $t0
.set zero_page_rreg,    $c17

.set UNROLL_FACTOR,     8

# Zeros page. I unrolled this a few times on point of principle. This is in no way optimal.

nano_zero_page:
    candperm    zero_page_ctmp_1, $kdc, $zero # Get this right...
    cfromptr    zero_page_ctmp_1, zero_page_ctmp_1, $zero
    daddiu      zero_page_tmp_1, zero_page_arg, (PHY_PAGE_SIZE) - (UNROLL_FACTOR * CAP_SIZE)

1:
    csc         zero_page_ctmp_1, zero_page_tmp_1, 0($kdc)
    csc         zero_page_ctmp_1, zero_page_tmp_1, (1*CAP_SIZE)($kdc)
    csc         zero_page_ctmp_1, zero_page_tmp_1, (2*CAP_SIZE)($kdc)
    csc         zero_page_ctmp_1, zero_page_tmp_1, (3*CAP_SIZE)($kdc)
    csc         zero_page_ctmp_1, zero_page_tmp_1, (4*CAP_SIZE)($kdc)
    csc         zero_page_ctmp_1, zero_page_tmp_1, (5*CAP_SIZE)($kdc)
    csc         zero_page_ctmp_1, zero_page_tmp_1, (6*CAP_SIZE)($kdc)
    csc         zero_page_ctmp_1, zero_page_tmp_1, (7*CAP_SIZE)($kdc)
    bne         zero_page_tmp_1,  zero_page_arg, 1b
    daddiu      zero_page_tmp_1, zero_page_tmp_1, -(UNROLL_FACTOR * CAP_SIZE)

    jr          $ra

#################################################################
# nano_kernel_init(register_t unmanaged_space, register_t return_addr, packaged args)
.global nano_kernel_init
nano_kernel_init:
################################################################
	# Populate exception registers: $kdc and $kcc
	cgetpccsetoffset $kcc, $zero                 # kdcc will hold the code global capability
	cgetdefault	$kdc                                    # kdc will hold the data global capability

    cmove       $c15, $c3                               # save this for the end

    dla         $k0, locals_start
    dli         $k1, locals_size
    csetoffset  $kr1c, $kdc, $k0
    csetbounds  $kr1c, $kr1c, $k1                       # kr1c will hold a capability to our locals

    csd         $zero, $zero, made_first_res($kr1c)

    dli         $k0, context_table
    dli         $k1, (N_CONTEXTS * CONTEXT_SIZE)
    csetoffset  $kr2c, $kr1c, $k0
    csetbounds  $kr2c, $kr2c, $k1                       # Create capability to context_table

    dli         $k0, CONTEXT_SIZE

    cincoffset  $c13, $kr2c, $k0
    csc         $c13, $zero, next_context($kr1c)        # Initialise next context

    csetbounds  $c3, $kr2c, $k0
    dli         $k0, CHERI_FRAME_SIZE
    csd         $zero, $k0, 0($c3)                      # Set state to 0
    cfromptr    $c4, $c4, $zero
    csc         $c4, $k0, CAP_SIZE($c3)                 # Set foundation ref to NULL
    csc         $c3, $zero, current_context($kr1c)       # Create first context and make it the current context


    dli         $k1, CONTEXT_TYPE
    csetoffset  $kr2c, $kdc, $k1

    cseal       $c3, $c3, $kr2c                          # Seal the first context

    cfromptr    $kr2c, $kr2c, $zero
    csc         $kr2c, $zero, exception_context($kr1c)   # Set exception context = NULL

    csd         $zero, $zero, exception_level($kr1c)     # Set exception level to 0
    csd         $zero, $zero, exception_cause($kr1c)     # Set exception cause to 0

    # First NANO_SIZE of physical pages to nano owned. Then a0/page_size pages to system owned. The rest are free
    # TODO check that a0 is rounded to a page

    dli         $t0, phy_page_table                     # t0 is start of book
    dli         $t1, (NANO_SIZE / PHY_PAGE_SIZE)        # t1 is number of pages for the nano
    dli         $t2, page_nano_owned
    dsrl        $t3, $a0, PHY_PAGE_SIZE_BITS           # t3 is number of pages for system

    csw         $t2, $t0, 0($kr1c)                      # book[0].state = nano owned
    csd         $t1, $t0, REG_SIZE($kr1c)               # book[0].len = nano pages

    dsll        $t2, $t1, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t0, $t0, $t2                           # t0 is the start of the systems pages
    dli         $t2, page_system_owned


    csw         $t2, $t0, 0($kr1c)                      # book[nano].state = system owned
    csd         $t3, $t0, (REG_SIZE)($kr1c)              # book[nano].len = system
    csd         $zero, $t0, (REG_SIZE * 2)($kr1c)       # book[nano].prev = 0

    dsll        $t2, $t3, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t0, $t0, $t2                           # t0 is the start of the free pages
    dli         $t2, (TOTAL_PHY_PAGES - (NANO_SIZE / PHY_PAGE_SIZE))
    dsubu       $t2, $t2, $t3

    csd         $t2, $t0, (REG_SIZE)($kr1c)             # book[nano + system].len = all - nano - system
    csd         $t1, $t0, (REG_SIZE*2)($kr1c)           # book[nano + system].prev = nano

    # We have a cheeky extra terminal page of size 1 that doesn't really exist
    dli         $t0, phy_page_table + (TOTAL_PHY_PAGES * PHY_PAGE_ENTRY_SIZE)
    dli         $t1, page_nano_owned
    daddiu      $t3, $t3, (NANO_SIZE / PHY_PAGE_SIZE)
    dli         $t2, 1

    csw         $t1, $t0, 0($kr1c)                      # book[end].status = nano owned
    csd         $t2, $t0, (REG_SIZE)($kr1c)             # book[end].len = 1
    csd         $t3, $t0, (REG_SIZE*2)($kr1c)           # book[end].prev = nano+system

    # Setup physical

    dli         $k0, NANO_KERNEL_TYPE
    csetoffset  $kr2c, $kdc, $k0                         # kr2c holds the sealing capability for our plt.got

.macro init_table name
    dla         $k0,   \name
    csetoffset  $c13, $kcc, $k0
    cseal       $c13, $c13, $kr2c
    csc         $c13, $k1, 0($kr1c)
    daddiu      $k1, CAP_SIZE
.endm

    dli         $k1, create_context_cap
    # Store a sealed capability for each nanokernel function in a table
    NANO_KERNEL_IF_LIST(INIT_TABLE_MACRO)

    # Pass a read-only capability to the cap table
    dli          $k0, create_context_cap
    dli          $k1, cap_table_end - create_context_cap
    csetoffset   $c1, $kr1c, $k0
    csetbounds   $c1, $c1, $k1
    dli          $k0, (Perm_Load | Perm_Load_Capability)
    candperm     $c1, $c1, $k0


    cseal       $c2, $kr1c, $kr2c                        # Pass a sealed capability to our locals

    # a0 is the start of nano kernel secured memory. a1 is an address we should return to.
    dli         $k0, DEF_DATA_PERMS

    #check a0 < PHY_MEM_SIZE - NANO_SIZE.
    dli         $t0, (PHY_MEM_SIZE - NANO_SIZE)
    sltu        $t0, $a0, $t0
    beq         $t0, $zero, nano_kernel_die
    nop

    # c0 will be a global data capability to the unsecured memory
    # c17 will be a global code capability to the unsecured memory, with the index passed
    dli         $t0, (PHY_MEM_START + NANO_SIZE)          # t0 = start of mem available to system
    daddu       $t2, $a0, $t0                             # t2 = start of free physical mem

    csetoffset  $c13, $kdc, $t0
    csetbounds  $c13, $c13, $a0
    candperm    $c13, $c13, $k0
    csetdefault $c13                                    # c13 is the beginning of unmanaged mem

    csetoffset  $c17, $kcc, $t0
    csetbounds  $c17, $c17, $a0
    csetoffset  $c17, $c17, $a1
    candperm    $c17, $c17, $k0

    # TODO: not include our reserved types
    dli         $t0, DEF_SEALING_PERMS
    candperm    $c4, $kdc, $t0
    # Now remove capabilities to the nano kernel. We should deny access to kernel regs
    # TODO: and the tlb

    cld         $a0, $zero, 0($c15)
    cld         $a1, $zero, REG_SIZE($c15)
    cld         $a2, $zero, 2*REG_SIZE($c15)
    cld         $a3, $zero, 3*REG_SIZE($c15)

    # The only registers not cleared will be
    # Kernel regs (not accessible outside this module)
    # pcc/c17 (will be the return address)
    # c0 default data
    # c1 a read only capability to nano kernel method table
    # c2 a data capability for the nano kernel
    # c3 the first context handle
    # c4 is a capability is a sealing capability for the type space
    # a0 is to allow boot to pass an argument to the kernel. Don't want capabilities or they would have to be checked.
    cclearlo    EncodeReg(all) & ~(EN6(c0, c1, c2, c3, c4, c5))
    cclearhi    EncodeReg(all) & ~(EN4(c17, kdc, kcc, c31))

    cjr         $c17
    nop



##########################################################################
# idc/kdc will provide us a capability to our locals in everything below #
##########################################################################

.text
#######################################
# capability obtain_super_powers(void)
.global obtain_super_powers
obtain_super_powers:
#######################################
    cgetbase    $t0, $c17
    cgetlen     $t1, $c17
    csetoffset  $c13, $kcc, $t0
    cgetoffset  $t2, $c17
    csetbounds  $c17, $c13, $t1
    cmove       $c3, $kdc
    csetoffset  $c17, $c17, $t2
    creturn

###################################
# page_t* get_book(void)
.global get_book
get_book:
###################################
    dli        $t0, phy_page_table
    dli        $t1, (TOTAL_PHY_PAGES) * PHY_PAGE_ENTRY_SIZE
    csetoffset $c3, $idc, $t0
    dli        $t2, Perm_Load
    csetbounds $c3, $c3, $t1
    candperm   $c3, $c3, $t2
    creturn

# Notes: Locks the page being split, but not the next page (which will have its back ptr updated #
#################################################################
# void split_phy_page_range(register_t pagen, register_t new_len)
.global split_phy_page_range
split_phy_page_range:
#################################################################
    beqz        $a1, split_phy_end
    dli         $t0, TOTAL_PHY_PAGES                # check index in range
    sltu        $t0, $a0, $t0
    beq         $t0, $zero, split_phy_end               # $a0 >= $t0
    dli         $t0, phy_page_table                     # $t0 = start of book
    dsll        $t1, $a0, PHY_PAGE_ENTRY_SIZE_BITS      # $t1 = offset for pagen
    daddu       $t0, $t0, $t1                           # $t0 = book + pagen

    cincoffset  $c3, $idc, $t0                          # c3 is book entry

    dli         $t8, page_transaction
1:  cllw        $t9, $c3                                # lock physcial page
    cscw        $t3, $t8, $c3
    beqz        $t3, 1b
    nop

    cld         $t2, $t0, (REG_SIZE)($idc)              # $t2 = book[pagen].len
    sltu        $t3, $a1, $t2
    beq         $t3, $zero, split_phy_end               # $a1 >= $t2
    dsll        $t3, $t2, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset  $c4, $c3, $t3                           # c4 is last book entry
    dsubu       $t3, $t2, $a1                           # $t3 is length of next block

    csd         $a1, $t0, (REG_SIZE)($idc)              # book[pagen].len = new_len

    dsll         $t1, $a1, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t0, $t0, $t1                           # t0 = book + pagen + new_len

    csw         $t8, $t0, 0($idc)                       # book[new].state = transaction
    csd         $t3, $t0, (REG_SIZE)($idc)              # book[new].len = book[pagen].len - new_len
    csd         $a0, $t0, (REG_SIZE*2)($idc)            # book[new].prev = pagen
    csw         $t9, $t0, 0($idc)                       # book[new].state = book[pagen].state

    daddu       $t1, $a0, $a1
    csd         $t1, $zero, (2*REG_SIZE)($c4)           # book[next].prev = pagen + new_len

split_phy_end:
    csw         $t9, $zero, 0($c3)                      # unlock page
    cclearlo    EN2(c3,c4)
    creturn

# Notes: Locks pagen and the next page. Not the page after that even though the back pointer is updated #
##############################################
# void merge_phy_page_range(register_t pagen)
.global merge_phy_page_range
merge_phy_page_range:
##############################################

    dli         $t0, TOTAL_PHY_PAGES                # check index in range
    sltu        $t0, $a0, $t0
    beq         $t0, $zero, merge_phy_page_range_end    # $a0 >= $t0
    dli         $t0, phy_page_table                     # $t0 = start of book
    dsll        $t1, $a0, PHY_PAGE_ENTRY_SIZE_BITS      # $t1 = offset for pagen
    daddu       $t0, $t0, $t1                           # $t0 = book + pagen

    dli         $a2, page_transaction
    cincoffset  $c3, $idc, $t0                          # c3 is pointer to first page
1:  cllw        $t8, $c3                                # t8 is state of first page
    cscw        $t3, $a2, $c3
    beqz        $t3, 1b
    nop

    cld         $t2, $t0, (REG_SIZE)($idc)              # $t2 = book[pagen].len
    beqz        $t2, merge_phy_er_1
    sll         $t1, $t2, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t1, $t1, $t0                           # t1 = book[pagen + len]

    cincoffset  $c4, $idc, $t1                          # c3 is pointer to first page
1:  cllw        $t9, $c4                                # t9 is state of second page
    cscw        $t3, $a2, $c4
    beqz        $t3, 1b
    nop

    bne         $t8, $t9, merge_phy_er_2
    cld         $t3, $t1, REG_SIZE($idc)                # t3 = len2

    beqz        $t3, merge_phy_er_2

    daddu       $t2, $t2, $t3
    sll         $t3, $t3, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t3, $t3, $t1                           # t3 = book[pagen + len + len2]

    csd         $zero, $t1, REG_SIZE($idc)              # book[page + len].len = 0
    csd         $t2, $t0, REG_SIZE($idc)                # book[page].len += len2
    csd         $a0, $t3, (2 * REG_SIZE)($idc)          # book[page + len + len2].prev = pagen

    b           merge_phy_er_1                          # finish by unlocking first page
    csw         $zero, $zero, 0($c4)                    # unlock second page

merge_phy_er_2:
    csw         $t9, $zero, 0($c4)
merge_phy_er_1:
    csw         $t8, $zero, 0($c3)
merge_phy_page_range_end:
    cclearlo    EN2(c3,c4)
    creturn

###################################
# ptable get_top_level_table(void)
.global get_top_level_table
get_top_level_table:
###################################
    dli         $t0, top_virt_page
    dli         $t1, PAGE_TABLE_SIZE
    cincoffset  $c3, $idc, $t0
    dli         $t0, VTABLE_TYPE_L0
    csetbounds  $c3, $c3, $t1
    csetoffset  $c4, $kdc, $t0
    cseal       $c3, $c3, $c4

    clearlo     EN1(c4)
    creturn

#####################################
# res_t make_first_reservation(void)
.global make_first_reservation
make_first_reservation:
#####################################
    # TODO we should actually revoke the initial reservation in case the boot loader made a mistake
    # TODO more atomicity
    cld         $t2, $zero, made_first_res($idc)
    bnez        $t2, make_first_er
    dli         $t2, 1
    csd         $t2, $zero, made_first_res($idc)  # can only be done once

    dli         $t2, VIRT_MEM_START
    dli         $t1, VIRT_MEM_SIZE
    csetoffset  $c3, $kdc, $t2
    dli         $t0, DEF_DATA_PERMS
    csetbounds  $c3, $c3, $t1
    candperm    $c3, $c3, $t0                     # c3 is a cap to all of virtual mem

    dli         $t0, RES_META_SIZE                # amount of space we lost to meta data
    dsubu       $t2, $t1, $t0                     # remaining length
    dli         $t1, RES_TYPE                     # sealing type

    csetoffset  $c4, $kdc, $t1
    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3)     # set state to open
    csd         $t2, $zero, (RES_LENGTH_OFFSET)($c3)        # set length
    csd         $zero, $zero, (RES_PID_OFFSET)($c3)         # set parent to none

    csetbounds  $c3, $c3, $t0
    cseal       $c3, $c3, $c4                     # handle

make_first_er:
    cclearlo    EN2(c4, c5)
    creturn

#####################################################################
# void context_switch(context_t restore_from, context_t*  store_to);
.global context_switch
context_switch:
#####################################################################

    # Enter an exception level to turn off interrupts. We must at the least switch from restore_from.
    # However we might switch to the exception_context

    dmfc0   $t1, $12
    ori     $t1, 2                                      # set SR(EXL)
    mtc0    $t1, $12

    dli     $k0, 0

context_switch_local_entry:                             # void context_switch_local_entry(reg_t cause)
    # We cant use $idc fo a bit as we may enter here from an exception, so we put it in $kr1c for now
    cmove   $kr1c, $idc
    # TODO unseal these (if sealed)
    cmove   $idc, $c18
    cmove   $epcc, $c17

context_switch_exception_entry:                         # kr1c should contain a good pointer to our locals
    clc     $kr2c, $zero, current_context($kr1c)

    save_reg_frame_idc $kr2c, $k1, $c1, $epcc, $idc     # Save state

    # Now we can undo our weird juggle and use $idc again
    cmove   $idc, $kr1c

    dli      $k1, CONTEXT_TYPE
    bnez     $k0, switch_exception                      # set to exception cause if we used the local entry
    csetoffset  $kr1c, $kdc, $k1                        # meant to be in delay

    cunseal  $c3, $c3, $kr1c

    # Save a context to
    cseal    $kr2c, $kr2c, $kr1c                        # seal old context
    csc      $kr2c, $zero, 0($c4)                       # save it in store_to


switch_restore:                                         # void switch_restore(context_t restore_from)

    csc      $c3, $zero, current_context($idc)          # set c3 it as the current context

    # Check if we had supressed any interrupts. If we did, we can pretty much treat this like the
    # Beginning of an exception and just restore the exception context

    cld     $k0,   $zero, exception_cause($idc)         # load cause
    csd     $zero, $zero, exception_cause($idc)         # and set to 0
    beqz    $k0,   switch_restore_final                 # no exception
    csd     $zero, $zero, exception_level($idc)         # critical_state.level = 0 (should be in delay)

switch_exception:                                       # void switch_exception(context_t victim, reg_t cause)
    csd     $zero, $zero, exception_cause($idc)
                # If we were doing it properly we would modify it to have the faulting instruction be from the new
                # activation. However, for now as only the bits to check the type of
                # interrupt we will just use this.

    clc     $c3, $zero, current_context($idc)           # Current context is the victim
    cld     $t0, $zero, exception_ccause($idc)          # load ccause
    cseal   $c4, $c3, $kr1c                             # Seal c3 to pass to exception handler
    csd     $k0, $zero, last_exception_cause($idc)      # store cause
    csc     $c4, $zero, victim_context($idc)            # store victim
    csd     $t0, $zero, last_exception_ccause($idc)     # store ccause
    dmfc0   $t0, cp0_badvaddr
    csd     $t0, $zero, last_bad_vaddr($idc)


    clc     $c3, $zero, exception_context($idc)         # load exception context
    csc     $c3, $zero, current_context($idc)           # set it as current context
    # HERE # If you want to restore the exception context with magic values, store via c3

    # Restore everything, we dont have a register spare for $c0 so set default while restoring
    # We use exception registers here. These are not used by the critical section check in exception.S

    .macro crestore_setc0 greg, offset, frame
        crestore \greg, \offset, \frame
        .if \offset == 0
            csetdefault \greg
        .endif
    .endm

switch_restore_final:
    dli         $a0, CHERI_FRAME_SIZE
    cld         $a0, $a0, 0($c3)
    bnez        $a0, nano_kernel_die                    # Check this context is still alive

    # c0 is not really stored in $c1, we just use it as temporary. $c3 will be overwritten so use $kr1c.
    cmove $kr1c, $c3
    restore_reg_frame_gen crestore_setc0, grestore, $kr1c, $t0, $c1, $epcc

return:
    # This tackles a qemu bug
    cgetoffset $k1, $epcc
    dmtc0      $k1, $14

    beqz       $k0, normal_return
    dmfc0      $k0, $12

exceptional_return:
    dli        $k1, 1                                   # disable interrupts otherwise the exception handler
    not        $k1, $k1                                 # will may immediately have one
    and        $k0, $k0, $k1
    mtc0       $k0, $12
    eret

normal_return:
    ori        $k0, $k0, 1                              # enable interupts again
    mtc0       $k0, $12
    eret

#################################################
# void get_last_exception(exection_cause_t* out)
.global get_last_exception
get_last_exception:
#################################################
    clc        $c4, $zero, victim_context($idc)
    cld        $t0, $zero, last_exception_cause($idc)
    cld        $t1, $zero, last_exception_ccause($idc)
    cld        $t2, $zero, last_bad_vaddr($idc)
    csc        $c4, $zero, 0($c3)
    csd        $t0, $zero, CAP_SIZE($c3)
    csd        $t1, $zero, (CAP_SIZE+REG_SIZE)($c3)
    csd        $t2, $zero, (CAP_SIZE+(2*REG_SIZE))($c3)
    creturn

################################################################################################################
# capability get_phy_page(register_t page_n, register_t cached, register_t npages, cap_pair* out, register_t IO)
.global get_phy_page
get_phy_page:
################################################################################################################

    beqz        $a2, get_phy_page_end
    dli         $t0, TOTAL_PHY_PAGES
    cmove       $c5, $c3
    sltu        $t0, $a0, $t0
    beq         $t0, $zero, get_phy_page_end            # $a0 >= $t0
    dsll         $t0, $a0, PHY_PAGE_ENTRY_SIZE_BITS
    dli         $t1, phy_page_table
    daddu       $t0, $t0, $t1                           # $t0 is the offset of our phy entry
    cld         $t1, $t0, REG_SIZE($idc)                # $t1 = len of record

    bne         $t1, $a2, get_phy_page_end              # $t1 must be npages
    clw         $t1, $t0, 0($idc)                       # $t1 = state
    bnez        $t1, get_phy_page_end                   # state must be un-used

    dli         $t1, page_system_owned
    dli         $t2, page_io
    movn        $t1, $t2, $a3

    csw         $t1, $t0, 0($idc)                       # set to system owned / io

    bnez        $a1, 1f                                 # 0: uncached. ow: cached
    dli         $t1, PHY_MEM_START_TOP
    dli         $t1, PHY_MEM_START_UNCACHED_TOP
    1: dsll     $t1, $t1, TOP_ADDR_SHIFT

    dsll        $t2, $a0, PHY_PAGE_SIZE_BITS
    dsll        $t0, $a2, PHY_PAGE_SIZE_BITS
    daddu       $t1, $t1, $t2                           # t1 is the offset required for our cap
    csetoffset  $c3, $kdc, $t1
    csetoffset  $c4, $kcc, $t1
    csetbounds  $c3, $c3, $t0                           # bounds number of pages requested
    csetbounds  $c4, $c4, $t0

    dli         $t0, DEF_DATA_PERMS   #t0 is the permission mask
    dli         $t1, (Perm_Load | Perm_Store)
    movn        $t0, $t1, $a3

    candperm    $c3, $c3, $t0
    candperm    $c4, $c4, $t0

    csc         $c3, $zero, CAP_SIZE($c5)
    csc         $c4, $zero, 0($c5)

get_phy_page_end:
    creturn


########################################################
# context_t create_context(reg_frame_t* initial_state);
.global create_context
create_context:
########################################################

# first check alignment
    cgetbase    $t0, $c3
    cgetoffset  $t1, $c3
    daddu       $t0, $t0, $t1
    andi        $t0, $t0, (CAP_SIZE-1)
    bnez        $t0, 1f


    dli         $t0,  CONTEXT_SIZE                          # TODO atomic increment
    clc         $c4,  $zero, next_context($idc)
    cincoffset  $c5,  $c4, $t0
    csc         $c5,  $zero, next_context($idc)             # increment next context



    csetbounds  $c4, $c4, $t0
    dli         $a0, CHERI_FRAME_SIZE
    csd         $zero, $a0, 0($c4)                          # set state to allocated for new context
    cfromptr    $c5, $c5, $zero
    csc         $c5, $a0, CAP_SIZE($c4)                     # set foundation ref to null
    # memcpy a0 bytes from c3 to c4
    dli         $t0, 0

memcpy_loop:
    clc         $c5, $t0, 0($c3)
    daddiu      $t0, $t0, CAP_SIZE
    bne         $t0, $a0, memcpy_loop
    csc         $c5, $t0, (-CAP_SIZE)($c4)

    dli         $t0, CONTEXT_TYPE
    csetoffset  $c3, $kdc, $t0                             # Load sealing capability
    cseal       $c3, $c4, $c3                              # Return sealed cap

1:
    cclearlo    EN3(c4,c5,c6)                              # c3 is a return value
    creturn



########################################################################
# context_t destroy_context(context_t context, context_t restore_from);
.global destroy_context
destroy_context:
########################################################################

    dli         $t0, CONTEXT_TYPE
    csetoffset  $c13, $kdc, $t0                             # Load sealing capability
    cunseal     $c3, $c3, $c13                              # unseal context we are destroying
    clc         $c14, $zero, current_context($idc)          # load current context
    ceq         $t1, $c3, $c14                              # t1 = 1 if we are deleting ourselves
    dli         $t2, 1
    dli         $t0, CHERI_FRAME_SIZE
    beqz        $t1, destroy_context_end
    csd         $t2, $t0, 0($c3)                            # set state to dead (in delay slot)

    # If we are here we are destroying ourselves, thus we should restore restore_from
    cunseal     $c3, $c4, $c13
    dmfc0       $t1, $12
    ori         $t1, 2                                      # set SR(EXL)
    mtc0        $t1, $12
    j           switch_restore
    nop

destroy_context_end:
    cclearlo    (1 << 13) | (1 << 14) | (1 << 3)
    creturn

################################
.global get_critical_level_ptr
get_critical_level_ptr:
################################

    dli        $t0, exception_level
    dli        $t1, REG_SIZE
    cincoffset $c3, $idc, $t0
    dli        $t2, DEF_DATA_PERMS
    csetbounds $c3, $c3, $t1
    candperm   $c3, $c3, $t2
    creturn

################################
.global get_critical_cause_ptr
get_critical_cause_ptr:
################################

    dli        $t0, exception_cause
    dli        $t1, REG_SIZE
    cincoffset $c3, $idc, $t0
    dli        $t2, DEF_DATA_PERMS
    csetbounds $c3, $c3, $t1
    candperm   $c3, $c3, $t2
    creturn

#################################
# void critical_section_enter();
.global critical_section_enter
critical_section_enter:
#################################

    cld        $v0, $zero, exception_level($idc)            # TODO atomic increment
    daddiu     $v0, 1
    csd        $v0, $zero, exception_level($idc)
    creturn




#################################
# void critical_section_exit();
.global critical_section_exit
critical_section_exit:
#################################

    cld        $v0, $zero, exception_level($idc)            # TODO atomic decrement
    daddiu     $v0, -1
    bnez       $v0, kernel_critical_section_exit_end
    csd        $v0, $zero, exception_level($idc)            # decrement level

    cld        $t0, $zero, exception_cause($idc)
    beqz       $t0, kernel_critical_section_exit_end
    csd        $zero, $zero, exception_cause($idc)             # and set cause to 0

    dmfc0      $t1, $12
    ori        $t1, 3                                       # set SR(EXL) and SR(IE)
    mtc0       $t1, $12
    j          context_switch_local_entry                   # calling context switch with k0 = cause will
    move       $k0, $t0                                     # will switch to the exception context

kernel_critical_section_exit_end:
    creturn




#################################################
# void set_exception_handler(context_t context);
.global set_exception_handler
set_exception_handler:
#################################################
    dli         $t0, CONTEXT_TYPE
    csetoffset  $c13, $kdc, $t0
    cunseal     $c13, $c3, $c13
    csc         $c13, $zero, exception_context($idc)
    creturn


#############################
# void nano_kernel_die(void)
.global nano_kernel_die
nano_kernel_die:
#############################
    li  $zero, 0xbeef
    li  $v0,   0xbad
    dli $k0,  ((0x1f000000 + 0x00500) | 0x9000000000000000)
    li  $k1,  0x42
    csb $k1, $k0, 0($kdc)

#############################################
# capability get_userdata_for_res(res_t res)
.global get_userdata_for_res
get_userdata_for_res:
#############################################
    dli         $t0, RES_TYPE
    dli         $t1, RES_PRIV_SIZE
    csetoffset  $c4, $kdc, $t0
    dli         $t2, RES_USER_SIZE
    cunseal     $c3, $c3, $c4
    cincoffset  $c3, $c3, $t1
    csetbounds  $c3, $c3, $t2
    cclearlo    EN1(c4)
    creturn

############################################
# void rescap_take(res_t res, cap_pair* out)
.global rescap_take
rescap_take:
############################################
    dli         $t0, RES_TYPE
    cmove       $c5, $c4
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res
    cgetoffset  $t3, $c3                    # t3 is non zero if using subfields
    csetoffset  $c3, $c3, $zero

    cgetbase    $t0, $c3
    daddiu      $t0, $t0, RES_META_SIZE     # get base
    cld         $t1, $zero, RES_LENGTH_OFFSET($c3)      # get length


    bnez        $t3, take_sub
    LOAD_RES_STATE $t2, $zero, RES_STATE_OFFSET($c3)          # get state

    bnez        $t2, take_er                # error if not open
    dli         $t2, res_taken

    STORE_RES_STATE $t2, $zero, RES_STATE_OFFSET($c3)   # set to taken
    dli         $t2, ~0
    csd         $t2, $zero, RES_SUBFIELD_BITMAP_OFFSET($c3)
    csw         $t2, $zero, (RES_SUBFIELD_BITMAP_OFFSET+8)($c3)
    b           take_make
    csh         $t2, $zero, (RES_SUBFIELD_BITMAP_OFFSET+12)($c3)    # set bitfield to all taken

take_sub:
# t0 is base of ALL of the reservation. t1 is length of ALL. t2 is state. t3 is index+1.
    dli         $t8, res_taken
    bne         $t2, $t8, take_er           # subfields are marked taken. We should look at the bitmap.
    daddiu      $t3, $t3, -1

    # first calculate byte and mask
    dli         $t2, 1
    andi        $t8, $t3, 0b111
    dsllv       $t8, $t2, $t8               # t8 is 1 byte mask for bitfield
    dsrl        $t2, $t3, 3                 # t2 is byte

    # set bit or error if already set
    clb         $t9, $t2, RES_SUBFIELD_BITMAP_OFFSET($c3)
    and         $a0, $t8, $t9
    bnez        $a0, take_er
    or          $t9, $t9, $t8
    csb         $t9, $t2, RES_SUBFIELD_BITMAP_OFFSET($c3)

    # calculate base offset
    clb         $t2, $zero, RES_SUBFIELD_SIZE_OFFSET($c3)   # t2 is the scale
    daddiu      $t8, $t3, 1
    dsllv       $t3, $t3, $t2                               # baseoff
    dsllv       $t8, $t8, $t2                               # bound

    # check bound not greater than total length
    sltu        $t9, $t1, $t8
    bnez        $t9, take_er

    # otherwise correctly calculate capability base+bounds to generate
    daddu       $t0, $t0, $t3
    dsubu       $t1, $t8, $t3

# t0 is base. t1 is length. #
take_make:
    dli         $t2, DEF_DATA_PERMS

    csetoffset  $c3, $kcc, $t0              # c3 is code
    csetoffset  $c4, $kdc, $t0              # c4 is data

    csetbounds  $c3, $c3, $t1
    csetbounds  $c4, $c4, $t1

    candperm    $c3, $c3, $t2
    candperm    $c4, $c4, $t2

    cbtu        $c5, take_end
    nop

    csc         $c3, $zero, 0($c5)
    csc         $c4, $zero, CAP_SIZE($c5)

take_end:
    creturn
take_er:
    clearlo     EN2(c3, c4)
    creturn

##################################################
# res_t rescap_getsub(res_t res, register_t index)
.global rescap_getsub
rescap_getsub:
##################################################

    cgetoffset  $t0, $c3
    bnez        $t0, getsub_er          # can only sub the main reservation
    sltiu       $t0, $a0, RES_SUBFIELD_BITMAP_BITS
    beqz        $t0, getsub_er          # can only have an index that will fit in the bitfield

    daddiu      $a0, $a0, 1             # 0 means not a subfield
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0
    cunseal     $c3, $c3, $c4
    csetoffset  $c3, $c3, $a0           # offset field encodes sub
    cseal       $c3, $c3, $c4

getsub_er:
    cclearlo    EN1(c4)
    creturn

####################################################
# res_t rescap_splitsub(res_t res, register_t scale)
.global rescap_splitsub
rescap_splitsub:
####################################################

    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res
    LOAD_RES_STATE $t0, $zero, RES_STATE_OFFSET($c3)            # get state
    bnez        $t0, take_er                                    # error if not open
    dli         $t0, res_taken
    STORE_RES_STATE $t0, $zero, RES_STATE_OFFSET($c3)           # set to taken

    csd         $zero, $zero, RES_SUBFIELD_BITMAP_OFFSET($c3)
    csw         $zero, $zero, (RES_SUBFIELD_BITMAP_OFFSET+8)($c3)
    csh         $zero, $zero, (RES_SUBFIELD_BITMAP_OFFSET+12)($c3)      # set bitfield to all not taken
    csb         $a0, $zero, (RES_SUBFIELD_SIZE_OFFSET)($c3)             # set scale

    cseal       $c3, $c3, $c4

    cclearlo    EN1(c4)
    creturn

####################################
# capability rescap_info(res_t res)
.global rescap_info
rescap_info:
####################################
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res

    cgetoffset  $t3, $c3                    # get sub-id if exists
    csetoffset  $c3, $c3, $zero
    cld         $t2, $zero, RES_LENGTH_OFFSET($c3)       # get length
    cgetbase    $t1, $c3
    daddiu      $t1, $t1, RES_META_SIZE

    beqz        $t3, normal_info
    csetoffset  $c5, $kdc, $t1

    clb         $t0, $zero, RES_SUBFIELD_SIZE_OFFSET($c3)
    dli         $t2, 1
    dsllv       $t2, $t2, $t0               # set new bounds
    daddiu      $t3, $t3, -1
    dsllv       $t3, $t3, $t0
    cincoffset  $c5, $c5, $t3               # increment to get sub

    normal_info:
    dli         $t0, DEF_DATA_PERMS
    csetbounds  $c3, $c5, $t2
    candperm    $c3, $c3, $t0
    ccleartag   $c3, $c3

    cclearlo    EN1(c4)
    creturn


.macro EXTRACT_AND_SCALE_L0 addr, out, scale
    dsll        \out, \addr, CHECKED_BITS                                             # clear top bits
    dsrl        \out, \out, (CHECKED_BITS + L1_BITS + L2_BITS + UNTRANSLATED_BITS)    # clear lower bits
    dsll        \out, \out, \scale                                                    # scale
.endm

.macro EXTRACT_AND_SCALE_L1 addr, out, scale
    dsll        \out, \addr, CHECKED_BITS + L0_BITS                                   # clear top bits
    dsrl        \out, \out, (CHECKED_BITS + L0_BITS + L2_BITS + UNTRANSLATED_BITS)    # clear lower bits
    dsll        \out, \out, \scale                                                    # scale
.endm

.macro EXTRACT_AND_SCALE_L2 addr, out, scale
    dsll        \out, \addr, CHECKED_BITS + L1_BITS + L0_BITS                          # clear top bits
    dsrl        \out, \out, (CHECKED_BITS + L1_BITS + L0_BITS + UNTRANSLATED_BITS)     # clear lower bits
    dsll        \out, \out, \scale                                                    # scale
.endm


######################################
# void rescap_revoke_start(res_t res);
.global rescap_revoke_start
rescap_revoke_start:
######################################
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res

    cgetoffset  $t0, $c3
    bnez        $t0, revoke_er
    dli         $t1, res_taken

    LOAD_RES_STATE $t0, $zero, RES_STATE_OFFSET($c3)
    bne         $t0, $t1, revoke_er        # check this reservation is taken

    cld         $t0, $zero, revoke_state($idc)
    bnez        $t0, revoke_er             # check we are not already revoking
    dli         $t2, res_revoking

    ctoptr      $a2, $c3, $kdc              #a2 is the base of our revoke
    cld         $t1, $zero, RES_LENGTH_OFFSET($c3)
    daddiu      $a4, $t1, RES_META_SIZE     #a4 is the size of revoke
    addu        $a3, $a2, $a4               #a3 is the end of our revoke (not inclusive)

    andi        $t0, $a2, (PHY_PAGE_SIZE-1)
    bnez        $t0, revoke_er              # base of revocation must be page aligned
    andi        $t0, $a4, (PHY_PAGE_SIZE-1)
    bnez        $t0, revoke_er              # size of revocation must be page aligned

    li          $t1, 1
    li          $t2, res_revoking
    csd         $t1, $zero, revoke_state($idc) # Set revoke state to 1
    STORE_RES_STATE $t2, $zero, RES_STATE_OFFSET($c3) # Mark reservation as revoking

    csd         $a2, $zero, revoke_base($idc)
    csd         $a3, $zero, revoke_bound($idc)

    cclearlo    EN4(c3, c4, c5, c6)
    creturn



#######################################
# void rescap_revoke_finish(res_t res);
.global rescap_revoke_finish
rescap_revoke_finish:
#######################################

    cld         $t0, $zero, revoke_state($idc)
    daddiu      $t0, $t0, -1
    bnez        $t0, revoke_er
    li          $t0, 2
    csd         $t0, $zero, revoke_state($idc)  # set revoke state to 2

    cld         $a2, $zero, revoke_base($idc)
    cld         $a3, $zero, revoke_bound($idc)
    dsubu       $a4, $a3, $a2

    # TODO check vtables (should all be -1, or 0 and then set to -1
    dli         $t0, top_virt_page
    cincoffset  $c5, $idc, $t0              # c5 is the top level table
                                            # c6 is the L1 table. c7 the l2

    # make upper bound INCLUSIVE for easier calculations
    daddiu      $a3, $a3, (-UNTRANSLATED_PAGE_SIZE)

    # starting points for each level in t0, t1, and t2
    EXTRACT_AND_SCALE_L0 $a2, $t0, PAGE_TABLE_ENT_BITS
    EXTRACT_AND_SCALE_L1 $a2, $t1, PAGE_TABLE_ENT_BITS
    EXTRACT_AND_SCALE_L2 $a2, $t2, PAGE_TABLE_ENT_BITS

    # ending points for each level in t3, t8, t9
    EXTRACT_AND_SCALE_L0 $a3, $t3, PAGE_TABLE_ENT_BITS
    daddiu     $t3, $t3, PAGE_TABLE_ENT_SIZE
    # a1 has the used flag
    dli         $a1, VTABLE_ENTRY_USED

    # The following walks through the vtables and checks all entries covered by the reservation are used,
    # or if they are un-used sets them to used. If an L2 mapping is present, we get an error.

    l0_loop_start:

        cld         $a0, $t0, 0($c5)
        beq         $a0, $a1, l0_loop_footer    # if used everything is ok
        daddiu      $t0, $t0, PAGE_TABLE_ENT_SIZE
        beqz        $a0, 1f                     # if un-used must set to used

        cfromptr    $c6, $kdc, $a0              # otherwise go into inner loop
        bne         $t0, $t3, l1_loop_start
        dli         $t8, (PAGE_TABLE_ENT_PER_TABLE << PAGE_TABLE_ENT_BITS)
        EXTRACT_AND_SCALE_L1 $a3, $t8, PAGE_TABLE_ENT_BITS
        daddiu      $t8, $t8, PAGE_TABLE_ENT_SIZE

        l1_loop_start:

            cld         $a0, $t1, 0($c6)
            beq         $a0, $a1, l1_loop_footer    # if used everything is ok
            daddiu      $t1, $t1, PAGE_TABLE_ENT_SIZE
            beqz        $a0, 2f                     # if un-used must set to used

            cfromptr    $c7, $kdc, $a0              # otherwise go into inner loop
            bne         $t1, $t8, l2_loop_start
            dli         $t9, (PAGE_TABLE_ENT_PER_TABLE << PAGE_TABLE_ENT_BITS)
            EXTRACT_AND_SCALE_L2 $a3, $t9, PAGE_TABLE_ENT_BITS
            daddiu      $t9, $t9, PAGE_TABLE_ENT_SIZE
            l2_loop_start:

                cld         $a0, $t2, 0($c7)
                bne         $a0, $a1, 3f            # if used everything is ok
                daddiu      $t2, $t2, PAGE_TABLE_ENT_SIZE
                bne         $t2, $t9, l2_loop_start
                nop
                b           l1_loop_footer
                nop

                3:
                bnez        $a0, revoke_er         # A mapping still exists. Error.
                nop
                bne         $t2, $t9, l2_loop_start
                csd         $a1, $t2, (-PAGE_TABLE_ENT_SIZE)($c7)

            l2_loop_end:
                b           l1_loop_footer
                nop

        2:
            bnez        $t2, revoke_er             # check we arn't skipping the start
            daddiu      $t2, $t0, -PAGE_TABLE_ENT_SIZE
            dsll        $t2, $t2, (UNTRANSLATED_BITS + L2_BITS + L1_BITS - PAGE_TABLE_ENT_BITS)
            dsll        $a0, $t1, (UNTRANSLATED_BITS + L2_BITS - PAGE_TABLE_ENT_BITS)
            daddu       $a0, $a0, $t2
            daddiu      $a0, $a0, (-UNTRANSLATED_PAGE_SIZE) # This is the inclusive upper bound we would be freeing
            sltu        $a0, $a3, $a0              # when the address is greater than it should be, we have an error
            bnez        $a0, revoke_er        # the last page must have an explicit entry
            nop


            csd         $a1, $t1, (-PAGE_TABLE_ENT_SIZE)($c6)# if un-used set to used

        l1_loop_footer:
            bne         $t1, $t8, l1_loop_start
            dli         $t2, 0
        l1_loop_end:
            b           l0_loop_footer
            nop

    1:
        dsll        $a0, $t0, (UNTRANSLATED_BITS + L2_BITS + L1_BITS - PAGE_TABLE_ENT_BITS)
        daddiu      $a0, $a0, (-UNTRANSLATED_PAGE_SIZE) # This is the inclusive upper bound we would be freeing
        sltu        $a0, $a3, $a0              # when the address is greater than it should be, we have an error
        bnez        $a0, revoke_er        # the last page must have an explicit entry
        daddu       $a0, $t1, $t2
        bnez        $a0, revoke_er             # the first page must have an explicit entry, unless aligned well
        nop

        csd         $a1, $t0, (-PAGE_TABLE_ENT_SIZE)($c5) # if un-used set to used

    l0_loop_footer:
        dli         $t1, 0
        bne         $t0, $t3, l0_loop_start
        dli         $t2, 0
    l0_loop_end:

    # Make exclusive again
    daddiu      $a3, $a3, UNTRANSLATED_PAGE_SIZE

    # We are now revoking. No errors from now on.

    dli         $a0, PHY_MEM_START          #a0 is the start of memory
    dli         $a1, PHY_MEM_END            #a1 the end

#TRACE_ON

    # Blocking writing of the capability #
    dli          $t0, DEF_DATA_PERMS
    dmtc0       $a2, $30, 2
    dmtc0       $a3, $30, 3
    dmtc0       $t0, $30, 4                 # only revoke Writable and Executable caps, not sealing caps


    dsubu        $a1, $a1, $a0
    beqz        $a1, revoke_loop_end
    csetoffset  $c5, $kdc, $a0
    dli         $t0, CAP_SIZE
    dli         $t3, PHY_PAGE_SIZE
    dli         $t8, 0


revoke_loop_start:  # We will now revoke every capability with a2 <= base < a3.

    andi        $t1, $t8, (PHY_PAGE_SIZE-1)
    bnez        $t1, 1f
    dsrl        $t1, $t8, (PHY_PAGE_SIZE_BITS - PHY_PAGE_ENTRY_SIZE_BITS)
    dli         $t2, phy_page_table
    daddu       $t1, $t1, $t2

    # only bother scanning nano owned, mapped and system owned #
    # FIXME another thing that might race with updates. Maybe lock modifying the state of these pages #
    clw         $t2, $t1, 0($idc)
    cld         $t1, $t1, REG_SIZE($idc)    # Get length. if non zero use last state
    movn        $t9, $t2, $t1

    dli         $t2, page_nano_owned
    beq         $t9, $t2, 1f
    dli         $t2, page_system_owned
    beq         $t9, $t2, 1f
    dli         $t2, page_mapped
    beq         $t9, $t2, 1f
    nop

    # Otherwise skip this page
    daddu       $t8, $t8, $t3
    bne         $t8, $a1, revoke_loop_start
    cincoffset  $c5, $c5, $t3

    b           revoke_loop_end
    nop

1:  cllc        $c6, $c5
    cgetbase    $t1, $c6
    sltu        $t2, $t1, $a2
    bnez        $t2, revoke_loop_footer
    sltu        $t2, $t1, $a3
    beqz        $t2, revoke_loop_footer

    nop
    cscc        $t1, $c6, $c5               # Avoid trample of legitimate capability
    beqz        $t1, 1b                     # restart on fail
    nop

revoke_loop_footer:
    daddiu      $t8, $t8, CAP_SIZE
    bne         $t8, $a1, revoke_loop_start
    cincoffset  $c5, $c5, $t0

revoke_loop_end:
#TRACE_OFF

    # with multicore this will require a sync. This will force a switch to ourselves which will clear registers #

    dli         $t1, current_context
    dli         $t0, CONTEXT_TYPE
    cincoffset  $c4, $idc, $t1
    cincoffset  $c5, $kdc, $t0
    clc         $c3, $zero, current_context($idc)
    cseal       $c3, $c3, $c5

    cmove       $c13, $c17
    dla         $t0, revoke_restore
    b           revoke_restore
    cgetpccsetoffset    $c17, $t0

revoke_restore:
    cmove       $c17, $c13

    # Allow any cap writes again #
    dmtc0       $zero, $30, 4
    dmtc0       $zero, $30, 3
    dmtc0       $zero, $30, 2


    # Mark vtable entries as free

    dli         $t0, top_virt_page
    cincoffset  $c5, $idc, $t0              # c5 is the top level table
                                            # c6 is the L1 table. c7 the l2

    # make upper bound INCLUSIVE for easier calculations
    daddiu      $a3, $a3, (-UNTRANSLATED_PAGE_SIZE)

    # starting points for each level in t0, t1, and t2
    EXTRACT_AND_SCALE_L0 $a2, $t0, PAGE_TABLE_ENT_BITS
    EXTRACT_AND_SCALE_L1 $a2, $t1, PAGE_TABLE_ENT_BITS
    EXTRACT_AND_SCALE_L2 $a2, $t2, PAGE_TABLE_ENT_BITS

    # ending points for each level in t3, t8, t9
    EXTRACT_AND_SCALE_L0 $a3, $t3, PAGE_TABLE_ENT_BITS
    daddiu     $t3, $t3, PAGE_TABLE_ENT_SIZE
    # a1 has the used flag
    dli         $a1, VTABLE_ENTRY_USED

    # Another walk of the the vtables. This time we know they are either used or pointers to sub tables
    # This will set them to free so new tables can be attached. This is good as the next part of this
    # will cause a tlb miss
    clear_loop_l0_start:

        cld         $a0, $t0, 0($c5)
        bne         $a0, $a1, 1f    # if used set to free, otherwise recurse
        daddiu      $t0, $t0, PAGE_TABLE_ENT_SIZE

        # Error check edge cases #
        dsll        $a0, $t0, (UNTRANSLATED_BITS + L2_BITS + L1_BITS - PAGE_TABLE_ENT_BITS)
        daddiu      $a0, $a0, (-UNTRANSLATED_PAGE_SIZE) # This is the inclusive upper bound we would be freeing
        sltu        $a0, $a3, $a0              # if the address is greater than it should be, we have an error
        bnez        $a0, revoke_er        # the last page must have an explicit entry
        daddu       $a0, $t1, $t2
        bnez        $a0, revoke_er             # the first page must have an explicit entry, unless aligned well
                                               # when t1 and t2 are zero, this is not the first page, or its aligned

        dli         $t1, 0
        csd         $zero, $t0, (-PAGE_TABLE_ENT_SIZE)($c5)          # set to free
        bne         $t0, $t3, clear_loop_l0_start
        dli         $t2, 0
        b           clear_loop_l0_end
        nop
    1:
        cfromptr    $c6, $kdc, $a0              # l1 table
        bne         $t0, $t3, clear_loop_l1_start
        dli         $t8, (PAGE_TABLE_ENT_PER_TABLE << PAGE_TABLE_ENT_BITS)
        EXTRACT_AND_SCALE_L1 $a3, $t8, PAGE_TABLE_ENT_BITS
        daddiu      $t8, $t8, PAGE_TABLE_ENT_SIZE

        clear_loop_l1_start:

            cld         $a0, $t1, 0($c6)
            bne         $a0, $a1, 1f    # if used set to free, otherwise recurse
            daddiu      $t1, $t1, PAGE_TABLE_ENT_SIZE

            # Error check edge cases #
            bnez        $t2, revoke_er             # check we arn't skipping the start
            daddiu      $t2, $t0, -PAGE_TABLE_ENT_SIZE
            dsll        $t2, $t2, (UNTRANSLATED_BITS + L2_BITS + L1_BITS - PAGE_TABLE_ENT_BITS)
            dsll        $a0, $t1, (UNTRANSLATED_BITS + L2_BITS - PAGE_TABLE_ENT_BITS)
            daddu       $a0, $a0, $t2
            daddiu      $a0, $a0, (-UNTRANSLATED_PAGE_SIZE) # This is the inclusive upper bound we would be freeing
            sltu        $a0, $a3, $a0              # if the address is greater than it should be, we have an error
            bnez        $a0, revoke_er        # the last page must have an explicit entry
            nop

            csd         $zero, $t1, (-PAGE_TABLE_ENT_SIZE)($c6)          # set to free
        2:  bne         $t1, $t8, clear_loop_l1_start
            dli         $t2, 0
            bne         $t0, $t3, clear_loop_l0_start
            dli         $t1, 0
            b           clear_loop_l0_end
            nop
        1:
            cfromptr    $c7, $kdc, $a0              # l2 table
            bne         $t1, $t8, clear_loop_l2_start
            dli         $t9, (PAGE_TABLE_ENT_PER_TABLE << PAGE_TABLE_ENT_BITS)
            EXTRACT_AND_SCALE_L2 $a3, $t9, PAGE_TABLE_ENT_BITS
            daddiu      $t9, $t9, PAGE_TABLE_ENT_SIZE
            clear_loop_l2_start:
                daddiu      $t2, $t2, PAGE_TABLE_ENT_SIZE
                bne         $t2, $t9, clear_loop_l2_start
                csd         $zero, $t2, (-PAGE_TABLE_ENT_SIZE)($c7)
                b           2b
                nop

            clear_loop_l2_end:

        clear_loop_l1_end:

    clear_loop_l0_end:


    # Make exclusive again
    daddiu      $a3, $a3, UNTRANSLATED_PAGE_SIZE

    # Make a new reservation here now. This will cause a fault but the vtables should be available again #
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap

    dli         $t0, RES_META_SIZE
    csetoffset  $c3, $kdc, $a2
    csetbounds  $c3, $c3,  $a4
    sub         $a4, $a4, $t0               #some space for metadata

    dli         $t1, DEF_DATA_PERMS
    candperm    $c3, $c3, $t1

    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3)       # set state to open
    csd         $zero, $zero, (RES_PID_OFFSET)($c3)                   # set parent to 0
    csd         $a4, $zero, RES_LENGTH_OFFSET($c3)                    # set length

    cseal       $c3, $c3, $c4               # seal new reservation

    csd         $zero, $zero, revoke_state($idc) # Set revoke state to 0

    cclearlo    EN3(c4, c5, c6)
    creturn

revoke_er:
    cclearlo    EN4(c3, c4, c5, c6)
    creturn


###################################################
# res_t  rescap_split(capability res, size_t size)
.global rescap_split
rescap_split:
###################################################
    andi        $t0, $a0, (RES_META_SIZE-1) # check alignment
    bnez        $t0, split_er

    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res

    LOAD_RES_STATE $t0, $zero, RES_STATE_OFFSET($c3)          # get state
    bnez        $t0, split_er               # reservation must be open

    cld         $t1, $zero, (RES_LENGTH_OFFSET)($c3) # get length
    beqz        $t1, split_er               # length must be non zero

    daddiu      $a1, $t1, -(RES_META_SIZE)  # length minus new metadata space
    sltu        $t0, $a1, $a0
    bnez        $t0, split_er               # size arg too large

    dsubu       $a1, $a1, $a0                   # size of remainder
    cld         $t0, $zero, (RES_PID_OFFSET)($c3)       # get pid
    csd         $a0, $zero, (RES_LENGTH_OFFSET)($c3)    # set length for old res

    dli         $t8, RES_META_SIZE

    cgetbase    $t3, $c3
    daddiu      $t3, $t3, RES_META_SIZE
    daddu       $t3, $t3, $a0               # start of next reservation

    dli         $t1, DEF_DATA_PERMS

    csetoffset  $c3, $kdc, $t3              # Create new meta node
    csetbounds  $c3, $c3, $t8
    candperm    $c3, $c3, $t1

    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3)     # set state of new reservation to open
    csd         $a1, $zero, (RES_LENGTH_OFFSET)($c3)        # set length of new reservation
    csd         $t0, $zero, (RES_PID_OFFSET)($c3)           # set parent

    cseal        $c3, $c3, $c4
    cclearlo    EN1(c4)
    creturn

split_er:
    cclearlo    EN2(c3, c4)
    creturn


#############################################
# res_t rescap_merge(res_t res1, res_t res2)
.global rescap_merge
rescap_merge:
#############################################
    ceq         $t0, $c3, $c4
    bnez        $t0, merge_er               # naughty people may try trip us up merging a reservation with itself
    dli         $t0, RES_TYPE
    csetoffset  $c5, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c5
    cunseal     $c4, $c4, $c5               # unseal both arguments

    cgetoffset  $t1, $c3
    bnez        $t1, merge_er
    cgetoffset  $t1, $c4
    bnez        $t1, merge_er

    dli         $t0, res_taken

    LOAD_RES_STATE $t1, $zero, RES_STATE_OFFSET($c3)
    bne         $t0, $t1, merge_er          # check first is taken

    LOAD_RES_STATE $t1, $zero, RES_STATE_OFFSET($c4)
    bne         $t0, $t1, merge_er          # check second is taken

    cld         $t0, $zero, (RES_PID_OFFSET)($c3) # get depth of first
    cld         $t1, $zero, (RES_PID_OFFSET)($c4) # get depth of second
    bne         $t0, $t1, merge_er          # must have same depth

    cld         $t0, $zero, RES_LENGTH_OFFSET($c3)   # get first length
    cld         $t1, $zero, RES_LENGTH_OFFSET($c4)   # get second length
    daddiu      $t0, RES_META_SIZE
    cincoffset  $c6, $c3, $t0               # c6 should point to the next reservation
    cne         $t2, $c6, $c4               # if not equal then we are not merging adjacent blocks
    bnez        $t2, merge_er

    addu        $t0, $t0, $t1               # this is the total new length
    li          $t1, res_merged
    csd         $t0, $zero, RES_LENGTH_OFFSET($c3)      # store back in the field of the lower address reservation a new length.
    STORE_RES_STATE $t1, $zero, RES_STATE_OFFSET($c4)   # set higher address reservation to merged
    cseal       $c3, $c3, $c5               # although the caller can just use the first argument, its nice to return it
    cclearlo    EN3(c4, c5, c6)
    creturn

merge_er:
    cclearlo    EN4(c3, c4, c5, c6)
    creturn

#################################
# res_t rescap_parent(res_t res)
.global rescap_parent
rescap_parent:
#################################
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res
    LOAD_RES_STATE $t0, $zero, RES_STATE_OFFSET($c3)          # get state
    bnez        $t0, parent_er              # reservation must be open

    dli         $t2, res_taken
    STORE_RES_STATE $t2, $zero, RES_STATE_OFFSET($c3)          # set state field for parent

    cgetbase    $t0, $c3                    # parent base is id for child
    dli         $a0, RES_META_SIZE

    cld         $t1, $zero, (RES_LENGTH_OFFSET)($c3) # get length
    daddiu      $t2, $t1, (-RES_META_SIZE)  # length of child

    csetoffset  $c3, $kdc, $t0              # create new res node
    cincoffset  $c3, $c3, $a0
    csetbounds  $c3, $c3, $a0
    dli         $t3, DEF_DATA_PERMS
    candperm    $c3, $c3, $t3

    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3) # set state for child
    csd         $t2, $zero, (RES_LENGTH_OFFSET)($c3)    # set length for child
    csd         $t0, $zero, (RES_PID_OFFSET)($c3)       # set depth for child

    cseal       $c3, $c3, $c4               # seal child for return

    cclearlo    EN1(c4)
    creturn
parent_er:

    cclearlo    EN2(c3, c4)
    creturn

# Notes: will lock the physical page, and atomically updates the vtable entry #
########################################################################
# ptable create_table(register_t page, ptable parent, register_t index)
.global create_table
create_table:
########################################################################

    cgettype    $t0, $c3
    dli         $t2, VTABLE_TYPE_L0
    beq         $t0, $t2, good_level
    daddiu      $t2, 1
    bne         $t0, $t2, create_ptable_er
    daddiu      $t2, 1

good_level: #t2 now contains the type we will seal the next level with
    csetoffset  $c4, $kdc, $t0
    cunseal     $c3, $c3, $c4                           # unseal parent page

    sltiu       $t0, $a1, PAGE_TABLE_ENT_PER_TABLE      # check index in range
    beq         $t0, $zero, create_ptable_er            # $a1 >= $t0

    dli         $t0, TOTAL_PHY_PAGES                # check page number in range
    sltu        $t0, $a0, $t0
    beq         $t0, $zero, create_ptable_er            # $a0 >= $t0

    dsll         $t0, $a0, PHY_PAGE_ENTRY_SIZE_BITS
    dli         $t1, phy_page_table
    daddu       $t0, $t0, $t1                           # $t0 is the offset of our phy entry

    li          $t8, page_transaction
    cincoffset  $c5, $idc, $t0                          # c5 is out physical entry
2:  cllw        $t9, $c5                                # $t9 = state. Take out transaction lock.
    cscw        $t1, $t8, $c5
    beqz        $t1, 2b
    nop

    cld         $t1, $t0, REG_SIZE($idc)                # $t1 = len of record
    daddiu      $t1, $t1, -1
    bnez        $t1, create_ptable_er_restore_state     # $t1 must be 1
    nop

    beqz        $t9, 1f                                 # state must be un-used
    dli         $t8, page_ptable_free                   # or can safely re-use old ptables for new ptables
    bne         $t9, $t8, create_ptable_er_restore_state

1:

    dsll        $t0, $a0, PHY_PAGE_SIZE_BITS
    dli         $t1, PHY_MEM_START
    daddu       $t0, $t0, $t1                          # t0 is the address of the new page
    dsll        $t1, $a1, PAGE_TABLE_ENT_BITS          # t1 is index in the parent page to write to

    move        $t3, $ra
    jal         nano_zero_page                          # uses t8 and c13
    nop
    move        $ra, $t3

    # Check table not already created/deleted
    cincoffset  $c6, $c3, $t1                           # c6 is a pointer to vtable entry

3:  clld        $t3, $c6
    bnez        $t3, create_ptable_er_restore_state
    nop
    cscd        $t3, $t0, $c6                           # update vtable entry
    beqz        $t3, 3b


    dli         $t1, page_ptable
    csw         $t1, $zero, 0($c5)                      # set physical page as a table

    dli         $t1, PAGE_TABLE_SIZE                    # construct return value
    csetoffset  $c3, $kdc, $t0
    csetbounds  $c3, $c3, $t1
    csetoffset  $c4, $kdc, $t2
    cseal       $c3, $c3, $c4



    cclearlo    EN4(c4,c17, c5, c6)
    creturn

create_ptable_er_restore_state:
    csw         $t9, $zero, 0($c5)

create_ptable_er:
    cclearlo    EN5(c3, c17, c4, c5, c7)
    creturn


# checked untranslated # l0_index # l1_index # l2_index # unchecked untranslated bits

###########################################################
# ptable_t get_sub_table(ptable_t table, register_t index)
.global get_sub_table
get_sub_table:
###########################################################
    cgettype    $t0, $c3
    dli         $t2, VTABLE_TYPE_L0
    beq         $t0, $t2, 1f
    daddiu      $t2, 1
    bne         $t0, $t2, get_sub_table_er
    daddiu      $t2, 1

    1:
    csetoffset  $c4, $kdc, $t0
    cunseal     $c3, $c3, $c4                           # unseal parent page


    sltiu       $t0, $a0, PAGE_TABLE_ENT_PER_TABLE      # check index in range
    beq         $t0, $zero, get_sub_table_er            # $a0 >= $t0

    dsll        $t0, $a0, PAGE_TABLE_ENT_BITS
    csetoffset  $c4, $kdc, $t2
    cld         $t0, $t0, 0($c3)
    slti        $t2, $t0, -1
    beqz        $t2, get_sub_table_end
    cfromptr    $c3, $c3, $zero

    csetoffset  $c3, $kdc, $t0
    dli         $t1, PAGE_TABLE_SIZE
    csetbounds  $c3, $c3, $t1
    cseal       $c3, $c3, $c4

    get_sub_table_end:
    cclearlo    EN1(c4)
    creturn

    get_sub_table_er:
    cclearlo    EN2(c3,c4)
    creturn

###########################################################
# readable_table_t* get_read_only_table(ptable_t table,)
.global get_read_only_table
get_read_only_table:
###########################################################
    cgettype    $t0, $c3
    dli         $t1, -VTABLE_TYPE_L0
    daddu       $t1, $t1, $t0

    sltiu       $t1, $t1, VTABLE_LEVELS
    beqz        $t1, 1f

    csetoffset  $c4, $kdc, $t0
    dli         $t1, Perm_Load
    cunseal     $c3, $c3, $c4                           # Unseal table
    candperm    $c3, $c3, $t1

1:
    cclearlo    EN1(c4)
    creturn

# Notes: Locks the physical page, and atomically updates the vtable entry #
#########################################################################################
# void create_mapping(register_t page, ptable table, register_t index, register_t flags)
.global create_mapping
create_mapping:
#########################################################################################

    dli         $t0, VTABLE_TYPE_L2
    csetoffset  $c4, $kdc, $t0                          # unsealing cap for an L2 table
    cunseal     $c3, $c3, $c4

    sltiu       $t0, $a1, PAGE_TABLE_ENT_PER_TABLE
    beq         $t0, $zero, create_mapping_er           # $a1 >= $t0

    dsll         $t0, $a0, PHY_PAGE_ENTRY_SIZE_BITS
    dli         $t1, phy_page_table
    daddu       $t0, $t0, $t1                           # $t0 is the offset of our phy entry
    cincoffset  $c6, $idc, $t0                          # c6 is the phy entry cap
    dli         $t8, page_transaction

2:  cllw        $t9, $c6                                # $t9 is the state of this phy page
    cscw        $t0, $t8, $c6                           # lock phy entry
    beqz        $t0, 2b
    nop

    cld         $t1, $zero, REG_SIZE($c6)               # $t1 = len of record
    daddiu      $t1, $t1, -2
    bnez        $t1, create_mapping_er_restore_phy      # $t1 must be 2
    nop

    bnez        $t9, create_mapping_er_restore_phy      # must be un-used
    dsll        $t2, $a1, PAGE_TABLE_ENT_BITS           # t2 is offset of page entry to c3

    cincoffset  $c5, $c3, $t2
    dsll        $a0, $a0, PFN_SHIFT
    or          $a0, $a0, $a2

1:  clld        $t1, $c5                               # check -virtual- mapping is un-used
    bnez        $t1, create_mapping_er_restore_phy
    nop
    cscd        $t1, $a0, $c5                          # store EntryLo
    beqz        $t1, 1b

    dli         $t1, page_mapped
    csw         $t1, $zero, 0($c6)                     # set page to be mapped

    # TODO add autharising cap
    # TODO add the checked top bits

    cclearlo    EN4(c3,c4,c5,c6)
    creturn

create_mapping_er_restore_phy:
    csw         $t9, $zero, 0($c6)

create_mapping_er:
    cclearlo    EN4(c3,c4,c5,c6)
    creturn



######################################################
# void free_mapping(ptable_t table, register_t index)
.global free_mapping
free_mapping:
######################################################

    cgettype    $t0, $c3
    dli         $t1, -VTABLE_TYPE_L0
    daddu       $t1, $t1, $t0

    sltiu       $t2, $t1, VTABLE_LEVELS
    daddiu      $t1, $t1, -(VTABLE_LEVELS-1)             # t1 zero if last level
    beqz        $t2, free_er

    csetoffset  $c4, $kdc, $t0

    sltiu       $t0, $a0, PAGE_TABLE_ENT_PER_TABLE
    beq         $t0, $zero, free_er                     # $a0 >= PAGE_TABLE_ENT_PER_TABLE

    cunseal     $c3, $c3, $c4
    dsll        $t2, $a0, PAGE_TABLE_ENT_BITS           # t2 is offset of page entry to c3

    cld         $t3, $t2, 0($c3)                        # t3 is the PFN/Physical adress of page
    beqz        $t3, free_to_used                       # if free we can go straight to used w/o updating any physical pages
    dli         $t0, VTABLE_ENTRY_USED
    beq         $t0, $t3, free_er

    dli         $t9, 2
    dli         $a3, 0
    beqz        $t1, 1f
    dsrl        $t8, $t3, PFN_SHIFT                     # the last level has a PFN entry for the TLB
    dli         $a3, page_ptable_free
    dli         $t9, 1
    dsll        $t8, $t3, TOP_ADDR_BITS                 # other levels have physical indexs. first zero top.
    dsrl        $t8, $t8, TOP_ADDR_BITS + PHY_PAGE_SIZE_BITS # then shift down to get a pagen
    1:
    # t8 as the physical pagen. t9 has how many pages we will be freeing. a3 the type to set to.

    dsll        $t3, $t8, PHY_PAGE_ENTRY_SIZE_BITS
    dli         $t1, phy_page_table
    daddu       $t1, $t3, $t1                           # $t1 is the offset of our phy entry

    cld         $t3, $t1, REG_SIZE($idc)                # $t3 is the length
    bne         $t3, $t9, free_er
    nop

    csw         $a3, $t1, 0($idc)                       # store to update phy page status

free_to_used:
    csd         $t0, $t2, 0($c3)                        # store -1 in vtable to mean 'used'

    # Invalidate TLB - could do this selectively for performance

    .set N_TLB_ENTS, 32
    dli     $t0, N_TLB_ENTS

1:
    dmtc0   $t0, $0
    dmtc0   $t0, $10
    tlbwi
    bnez    $t0, 1b
    daddiu  $t0, -1

free_er:
    cclearlo    EN2(c3, c4)
    creturn



# Notes on calling rescap_take: provide c4 as null to avoid write. Will return in c3 and c4. Will use c5 #

.macro SUBROUTINE_TAKE
    cmove       $c14, $c17                          # save return code
    cmove       $c15, $c18                          # save return data

    dla         $t0, rescap_take
    cgetpccsetoffset $c12, $t0
    cjalr       $c12, $c17                          # call rescap_take
    cfromptr    $c4, $c4, $zero                     # null c4 for call to rescap_take

    cmove       $c17, $c14                          # restore return code
    cmove       $c18, $c15                          # restore return data
.endm

#struct foundation_metadata {
#    found_id_t id;
#    capability data;
#    capability entrys[n_entries];
#};

###########################################################################################
# entry_t foundation_create(res_t res, size_t image_size, capability image,
#                           size_t entry0, size_t n_entries)
.global foundation_create
foundation_create:
###########################################################################################

    beqz        $a2, found_er                       # must have at least 1 entry slot
    andi        $t0, $a0, (CAP_SIZE-1)
    bnez        $t0, found_er                       # make sure we copying a multiple of a cap
    cgetbase    $t0, $c4
    cgetoffset  $t1, $c4
    daddu       $t0, $t0, $t1
    andi        $t0, $t0, (CAP_SIZE-1)
    bnez        $t0, found_er                       # check source alignment
    nop

    cmove       $c13, $c4                           # save image

    SUBROUTINE_TAKE

    cbtu        $c3, found_er

    # check res size
    cgetlen     $t0, $c3

    # calculate meta size
    dsll        $t1, $a2, CAP_SIZE_BITS
    daddiu      $t1, $t1, FOUNDATION_ID_SIZE

    # check meta+image <= res size
    daddu       $t2, $t1, $a0
    sltu        $t3, $t0, $t2
    bnez        $t3, found_er

    # store other id fields
    csd         $a0, $zero, FOUNDATION_ID_LEN_OFFSET($c4)                   # id length field
    csd         $a1, $zero, FOUNDATION_ID_E0_OFFSET($c4)                    # id entry0 field
    csd         $a2, $zero, FOUNDATION_ID_NENT_OFFSET($c4)                  # if nentries field

    cmove       $c14, $c4                           # save c4 for later
    # store data we will get on entry
    cincoffset  $c4, $c4, $t1                       # c4 is the start of the user code/data
    dli         $t0, DEF_DATA_PERMS
    csetbounds  $c4, $c4, $a0                       # c4 is users data
    candperm    $c4, $c4, $t0
    csc         $c4, $zero, FOUNDATION_META_DATA_OFFSET($c14) # store data for invocation

    # create first entry point
    cincoffset  $c3, $c3, $t1                       # skip over metadata
    csetbounds  $c3, $c3, $a0                       # correctly bounded code pointer
    csetoffset  $c3, $c3, $a1                       # correctly offset entry

    csc         $c3, $zero, FOUNDATION_META_ENTRY_VECTOR_OFFSET($c14) # store first entry trampoline

    # create return token
    csetbounds  $c15, $c14, $t1                       # we can set offset to 0 to get metadata
    dli         $t3, FOUNDATION_META_ENTRY_VECTOR_OFFSET
    csetoffset  $c15, $c15, $t3                       # entry points to pcc to load in trampoline

    dli         $t3, FOUND_ENTRY_TYPE
    csetoffset  $c3, $kdc, $t3
    cseal       $c15, $c15, $c3

    # memcpy into image calculating sha256 as we go. c3 = source. c4 = dest. a0 = size.
    # Normal ABI, but saves c13 to c16 for us (how kind)
    cmove       $c3, $c13
    cmove       $c13, $c17          # save c17
    cmove       $c16, $c12          # save c12

    dla         $t0, sha256_start   # jump to sha256 routine
    cgetpccsetoffset $c12, $t0
    cjalr       $c12, $c17
    nop

    # restore c17/c12
    cmove       $c17, $c13
    cmove       $c12, $c16

    # store hash
    csd         $v0, $zero, 0($c14)
    csd         $v1, $zero, 8($c14)
    csd         $t0, $zero, 16($c14)
    csd         $t1, $zero, 24($c14)

    # get back return value we calculated before the hash
    cmove       $c3, $c15

    cclearlo    EN5(c4,c5,c13,c14,c15)
    creturn

    found_er:
    cclearlo    EN6(c3,c4,c5,c13,c14,c15)
    creturn


###########################################################################################
# void foundation_enter(entry_t entry)
.global foundation_enter
foundation_enter:
###########################################################################################

    dli         $t3, FOUND_ENTRY_TYPE
    csetoffset  $c4, $kdc, $t3
    cunseal     $c3, $c3, $c4

    clc         $c12, $zero, 0($c3)                     # get entry pcc
    csetoffset  $c3, $c3, $zero
    clc         $c4, $zero, current_context($idc)       # get current context
    dli         $t0, CHERI_FRAME_SIZE + CAP_SIZE
    clc         $idc, $zero, FOUNDATION_ID_SIZE($c3)    # get entry data

    csc         $c3, $t0, 0($c4)                        # set current context to point to id of foundation

    cjr         $c12
    cclearlo    EN2(c3,c4)


###########################################################################################
# void foundation_exit(void)
.global foundation_exit
foundation_exit:
###########################################################################################

    clc         $c3, $zero, current_context($idc)       # get current context
    dli         $t0, CHERI_FRAME_SIZE + CAP_SIZE
    cfromptr    $c4, $c4, $zero

    csc         $c4, $t0, 0($c3)
    cclearlo    EN2(c3,c4)
    creturn


###########################################################################################
# entry_t foundation_new_entry(size_t eid, capability at)
.global foundation_new_entry
foundation_new_entry:
###########################################################################################

    # Get found struct from current context
    dli         $t0, CHERI_FRAME_SIZE + CAP_SIZE
    clc         $c4, $zero, current_context($idc)
    clc         $c4, $t0, 0($c4)

    cbtu        $c4, new_entry_er
    nop

    # get entry count
    cld         $t0, $zero, FOUNDATION_ID_NENT_OFFSET($c4)
    sltu        $t0, $a0, $t0

    # bounds check
    beqz        $t0, new_entry_er

    dsll        $t0, $a0, CAP_SIZE_BITS
    daddiu      $t0, $t0, FOUNDATION_META_ENTRY_VECTOR_OFFSET
    cincoffset  $c4, $c4, $t0

    # set entry
    csc         $c3, $zero, 0($c4)

    # construct token
    li          $t0, FOUND_ENTRY_TYPE
    csetoffset  $c3, $kdc, $t0
    cseal       $c3, $c4, $c3

new_entry_er:
    cclearlo    EN1(c4)
    creturn


###########################################################################################
# cert_t rescap_take_cert(res_t res, cap_pair* out, register_t user_perms)
.global rescap_take_cert
rescap_take_cert:
###########################################################################################

    # get current foundation
    dli         $t0, CHERI_FRAME_SIZE + CAP_SIZE
    clc         $c5, $zero, current_context($idc)
    clc         $c5, $t0, 0($c5)
    cbtu        $c5, take_cert_er

    # make cert for current foundation
    dli         $t0, FOUNDATION_ID_SIZE
    csetbounds  $c5, $c5, $t0
    dli         $t0, Perm_Load
    candperm    $c5, $c5, $t0

    b           rescap_take_locked_cert_shared
    dli         $a1, FOUND_CERT_TYPE

###########################################################################################
# locked_t rescap_take_locked(res_t res, cap_pair* out, register_t user_perms, found_id_t* recipient_id)
.global rescap_take_locked
rescap_take_locked:
###########################################################################################

    dli         $a1, FOUND_LOCKED_TYPE

# rescap_take_locked_cert_shared(res_t res, cap_pair* out, register_t user_perms, found_id_t* id, size_t type_ret)
rescap_take_locked_cert_shared:

    cmove       $c13, $c4
    cmove       $c7, $c5        # c5 clobbered by take

    SUBROUTINE_TAKE

    cbtu        $c3, take_cert_er

    # c3 and c4 are code/data. c13 is what we plan to store by
    dli         $t0, RES_CERT_META_SIZE
    cgetlen     $t1, $c3
    daddiu      $t1, $t1, -RES_CERT_META_SIZE

    # make space for cert metadata
    cmove       $c6, $c4
    cincoffset  $c3, $c3, $t0
    cincoffset  $c4, $c4, $t0
    csetbounds  $c6, $c6, $t0
    csetbounds  $c3, $c3, $t1
    csetbounds   $c4, $c4, $t1

    # store cert
    csc         $c7, $zero, 0($c6)
    # store user code
    candperm    $c5, $c3, $a0
    csc         $c5, $zero, CAP_SIZE($c6)
    # store user data
    candperm    $c5, $c4, $a0
    csc         $c5, $zero, (2*CAP_SIZE)($c6)

    # store out code
    csc         $c3, $zero, 0($c13)
    # store out data
    csc         $c4, $zero, CAP_SIZE($c13)


    # construct token
    csetoffset  $c5, $kdc, $a1
    cseal       $c3, $c6, $c5

    cclearlo    EN7(c4,c5,c6,c7,c13,c14,c15)
    creturn

take_cert_er:
    cclearlo    EN8(c3,c4,c5,c6,c7,c13,c14,c15)
    creturn

###########################################################################################
# found_id_t* rescap_check_cert(cert_t cert, cap_pair* out)
.global rescap_check_cert
rescap_check_cert:
###########################################################################################

    # unlock
    dli         $t0, FOUND_CERT_TYPE
    csetoffset  $c5, $kdc, $t0
    cunseal     $c5, $c3, $c5

    # get user code
    clc         $c3, $zero, CAP_SIZE($c5)
    csc         $c3, $zero, 0($c4)
    # get user data
    clc         $c3, $zero, (2*CAP_SIZE)($c5)
    csc         $c3, $zero, CAP_SIZE($c4)
    # get found id
    clc         $c3, $zero, 0($c5)

    cclearlo    EN2(c4,c5)
    creturn


###########################################################################################
# void rescap_unlock(locked_t locked, cap_pair* out)
.global rescap_unlock
rescap_unlock:
###########################################################################################

    # unlock
    dli         $t0, FOUND_LOCKED_TYPE
    csetoffset  $c5, $kdc, $t0
    cunseal     $c3, $c3, $c5

    # get current foundation
    dli         $t0, CHERI_FRAME_SIZE + CAP_SIZE
    clc         $c5, $zero, current_context($idc)
    clc         $c5, $t0, 0($c5)
    cbtu        $c5, 1f

    # convert to same form as used for cert earlier
    dli         $t0, FOUNDATION_ID_SIZE
    csetbounds  $c5, $c5, $t0
    dli         $t0, Perm_Load
    candperm    $c5, $c5, $t0

    # load capability this is locked for
    clc         $c6, $zero, 0($c3)
    cexeq       $t0, $c6, $c5               # only proper guy can unlock
    beqz        $t0, 1f
    cmove       $c5, $c4

    # load return stuffs
    clc         $c4, $zero, (2*CAP_SIZE)($c3)
    clc         $c3, $zero, (CAP_SIZE)($c3)

    # store to out
    csc         $c3, $zero, 0($c5)
    csc         $c4, $zero, (CAP_SIZE)($c5)

    cclearlo    EN2(c5,c6)
    creturn

1:
    cclearlo    EN4(c3,c4,c5,c6)
    creturn








############################################
# Some stuff used by the exception vectors #
############################################

tlb_table_missing:
    cgetcause $k0
    dla $k1, locals_start
    csetoffset $kr1c, $kdc, $k1
    csd     $k0, $zero, exception_ccause($kr1c)
    b context_switch_exception_entry
    dmfc0 $k0, $13


get_interface:
    dla         $k0, locals_start
    dli         $k1, locals_size
    csetoffset  $kr1c, $kdc, $k0
    csetbounds  $kr1c, $kr1c, $k1                        # kr1c will hold a capability to our locals

    # TODO have a per context vector to say which functions are allowed
    dsll        $k1, $a1, CAP_SIZE_BITS

    clc         $c1, $k1, create_context_cap($kr1c)      # load code cap

    dli         $k0, NANO_KERNEL_TYPE
    csetoffset  $kr2c, $kdc, $k0                         # kr2c holds the sealing capability for our plt.got

    li          $k0, 4
    cseal       $c2, $kr1c, $kr2c
    cincoffset  $epcc, $epcc, $k0

    cgetoffset $k1, $epcc                                # counters an eret bug
    dmtc0      $k1, $14
    eret

.section .trampoline_exc, "ax"

###########################################################################################################
# Relocatable exception vector; checks we are not in a soft disable and then jumps to the context switcher
# normal program memory.  This runs with KCC installed in PCC.
		.global kernel_exception_trampoline
		.ent kernel_exception_trampoline
kernel_exception_trampoline:
###########################################################################################################

# cancel if we are in critical section.
# We assume that critical code never throws exceptions and only async interrupts happen.
# TODO: switch happens in an exception level, but other bits of the nano kernel may need thinking about
# TODO if an exception happens between nano begin and nano end we should just die.
        bnez      $a0, 1f
        dmfc0     $k0, $13
        andi      $k1, $k0, 0xFF
        daddiu    $k1, $k1, -(8 << 2) # syscall with a0 = 0 results in getting nano kernel IF
        beqz      $k1, get_interface
        cgetcause $k0
1:
        dla $k1, locals_start
        csetoffset $kr1c, $kdc, $k1
        cld $k1, $zero, exception_level($kr1c)
        beqz $k1, take_exception
        csd     $k0, $zero, exception_ccause($kr1c)

skip_exception:
        dmfc0   $k1, $13
        csd     $k1, $zero, exception_cause($kr1c)
        dmfc0   $k1, $12
        li      $k0, 1
        not     $k0, $k0
        and     $k1, $k1, $k0 # As interrupts had to be enabled
        mtc0    $k1, $12      # Disable interrupts, we are happy to take the hit as this is rare
        eret
take_exception:
		b context_switch_exception_entry
		dmfc0 $k0, $13
kernel_exception_trampoline_end:
		nop
		.global kernel_exception_trampoline_end
		.end kernel_exception_trampoline
		.size kernel_exception_trampoline, kernel_exception_trampoline_end - kernel_exception_trampoline

.section .trampoline_tlb, "ax"

################################################################################################
# Relocatable TLB filler; will try to do a fast replacement using xcontext
# o.w. will resume a handler context for a slow path
    .global tlb_trampoline
    .ent tlb_trampoline
tlb_trampoline: # TODO
################################################################################################
    dmfc0       $k0, cp0_badvaddr
    dla         $k1, top_virt_page_label                            # k1 = phy poiter to L0

    EXTRACT_AND_SCALE_L0 $k0, $k0, PAGE_TABLE_ENT_BITS
    daddu       $k1, $k1, $k0

    cld         $k1, $k1, 0($kdc)                                   # k1 = phy pointer to L1
    slti        $k0, $k1, -1
    beqz        $k0, tlb_table_missing

    dmfc0       $k0, cp0_badvaddr
    EXTRACT_AND_SCALE_L1 $k0, $k0, PAGE_TABLE_ENT_BITS
    daddu       $k1, $k1, $k0

    cld         $k1, $k1, 0($kdc)                                   # k1 = phy pointer to L2
    slti        $k0, $k1, -1
    beqz        $k0, tlb_table_missing

    dmfc0       $k0, cp0_badvaddr
    EXTRACT_AND_SCALE_L2 $k0, $k0, PAGE_TABLE_ENT_BITS
    daddu       $k1, $k1, $k0

    cld         $k1, $k1, 0($kdc)                                   # k1 = pfn entry
    slti        $k0, $k1, 1
    bnez        $k0, tlb_table_missing
    nop

    # Setup virtual page mask
    # dli         $k0, (1 << (PHY_PAGE_SIZE_BITS - 12)) - 1
    # TODO check the top bits of the vaddr

    dmtc0       $k1, cp0_entrylo0
    # dmtc0       $k0, cp0_pagemask
    daddiu      $k1, $k1, (1 << PFN_SHIFT)                          # make second maping the next page
    dmtc0       $k1, cp0_entrylo1

    tlbwr                                                           # write tlb entry
    eret

tlb_trampoline_end:
    .global tlb_trampoline_end
    .end tlb_trampoline
    .size tlb_trampoline, tlb_trampoline_end - tlb_trampoline

nano_kernel_end:
.size nano_kernel, nano_kernel_end - nano_kernel_start