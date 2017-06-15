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
#};

.set CONTEXT_SIZE, CHERI_FRAME_SIZE + CAP_SIZE


.set N_CONTEXTS,         64             # We never reallocate these, so its currently a huge limitation

#struct res_t {
#    enum state{open, taken, merged, collecting}
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
local_reg_var collection_state

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

.set zero_page_tmp_1,   $t1
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

    cjr         zero_page_rreg

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
    csd         $zero, $k0, 0($c3)
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

    cld         $t2, $t0, (REG_SIZE)($idc)              # $t2 = book[pagen].len
    sltu        $t3, $a1, $t2
    beq         $t3, $zero, split_phy_end               # $a1 >= $t2
    dsubu       $t3, $t2, $a1                           # $t3 is length of next block

    csd         $a1, $t0, (REG_SIZE)($idc)              # book[pagen].len = new_len

    dsll         $t1, $a1, PHY_PAGE_ENTRY_SIZE_BITS
    clw         $t2, $t0, 0($idc)                       # $t2 sate of old block
    daddu       $t0, $t0, $t1                           # t0 = book + pagen + new_len

    csw         $t2, $t0, 0($idc)                       # book[new].state = book[pagen].state
    csd         $t3, $t0, (REG_SIZE)($idc)              # book[new].len = book[pagen].len - new_len
    csd         $a0, $t0, (REG_SIZE*2)($idc)            # book[new].prev = pagen

    dsll         $t3, $t3, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t0, $t0, $t3                           # $t0 = book + pagen + oldlen
    daddu       $t1, $a0, $a1

    csd         $t1, $t0, (2*REG_SIZE)($idc)            # book[next].prev = pagen + new_len

split_phy_end:
    creturn

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

    cld         $t2, $t0, (REG_SIZE)($idc)              # $t2 = book[pagen].len
    beqz        $t2, merge_phy_page_range_end
    sll         $t1, $t2, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t1, $t1, $t0                           # t1 = book[pagen + len]

    clw         $t8, $t0, 0($idc)                       # book[page].status
    clw         $t9, $t1, 0($idc)                       # book[pagen + len].status

    bne         $t8, $t9, merge_phy_page_range_end
    cld         $t3, $t1, REG_SIZE($idc)                # t3 = len2

    beqz        $t3, merge_phy_page_range_end

    daddu       $t2, $t2, $t3
    sll         $t3, $t3, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t3, $t3, $t1                           # t3 = book[pagen + len + len2]

    csd         $zero, $t1, REG_SIZE($idc)              # book[page + len].len = 0
    csd         $t2, $t0, REG_SIZE($idc)                # book[page].len += len2
    csd         $a0, $t3, (2 * REG_SIZE)($idc)          # book[page + len + len2].prev = pagen

merge_phy_page_range_end:
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
    # TODO we should actually collect the initial reservation in case the boot loader made a mistake
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
    csd         $zero, $zero, 0($c3)              # set state to open
    csd         $zero, $zero, (REG_SIZE*2)($c3)   # set parent to none
    cincoffset  $c5, $c3, $t0                     # cap for user range
    cseal       $c3, $c3, $c4                     # handle
    csetbounds  $c5, $c5, $t2                     # correct bounds to account for user data
    csc         $c5, $zero, 0($c5)

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

###############################################################
# capability get_phy_page(register_t page_n, register_t cached, register_t npages, cap_pair* out)
.global get_phy_page
get_phy_page:
###############################################################

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
    csw         $t1, $t0, 0($idc)                       # set to system owned

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

    dli         $t0, CONTEXT_TYPE
    csetoffset  $c13, $kdc, $t0                             # Load sealing capability
    cmove       $c4,  $c3
    dli         $t0,  CONTEXT_SIZE                          # TODO atomic increment
    clc         $c3,  $zero, next_context($idc)
    cincoffset  $c14, $c3, $t0
    csc         $c14, $zero, next_context($idc)             # increment next context



    csetbounds  $c3, $c3, $t0
    dli         $a0, CHERI_FRAME_SIZE
    csd         $zero, $a0, 0($c3)                          # set state to allocated for new context
    # FIXME we dont need all of memcpy_c. We should either remove the need for it (and use a coventional fork)
    # FIXME or inline and remove all the fluff we know we dont need
    dla         $t0,  memcpy_c
    cgetpccsetoffset  $c12, $t0
    cmove       $c15, $c17                                  # Save return address
    cjalr       $c12, $c17                                  # Copy initial state into new context
    nop                                                     # Only uses an extra c5. We will need to clear this too

    cmove       $c17,  $c15                                 # Restore return address

    cseal       $c3, $c3, $c13                              # Return sealed cap


create_context_end:
    cclearlo    EN6(c4,c5,c12,c13,c14,c15)                  # c3 is a return value
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
    cld         $t0, $zero, 0($c3)          # get state
    csetbounds  $c4, $c4, $zero             # clear our sealing cap
    bnez        $t0, take_er                # error if not open
    dli         $t0, res_taken

    clc         $c6, $zero, RES_META_SIZE($c3)   # load return capability from struct
    cgetlen     $t1, $c6
    csd         $t0, $zero, 0($c3)          # set to taken
    csd         $t1, $zero, REG_SIZE($c3)   # set length (needed for merge)


    cgetbase    $t0, $c6                    # We must rederive a cap to have the execute bit
    cgetlen     $t1, $c6
    csetoffset  $c4, $kcc, $t0
    dli         $t2, DEF_DATA_PERMS
    csetbounds  $c4, $c4, $t1
    candperm    $c4, $c4, $t2

    csc         $c6, $zero, CAP_SIZE($c5)
    csc         $c4, $zero, 0($c5)
    creturn
take_er:
    cfromptr    $c3, $c3, $zero
    creturn


####################################
# capability rescap_info(res_t res)
.global rescap_info
rescap_info:
####################################
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res
    cld         $t0, $zero, 0($c3)          # get state
    dli         $t1, RES_VIEW_TYPE
    bnez        $t0, info_not_open          # can also get info for open reservation
    csetoffset  $c4, $kdc, $t1              # create sealing cap

    b           info_end
    clc         $c3, $zero, RES_META_SIZE($c3) # load return capability from struct

info_not_open:
    dli         $t1, res_taken
    bne         $t0, $t1, info_er
    dli         $t1, RES_META_SIZE
    cld         $t2, $zero, REG_SIZE($c3)       # get length
    cincoffset  $c3, $c3, $t1
    csetbounds  $c3, $c3, $t2

info_end:
    cseal       $c3, $c3, $c4
    cfromptr    $c4, $c4, $zero             # clear our sealing cap
    creturn

info_er:
    cclearlo    EN2(c3,c4)
    creturn

##################################
# res_t rescap_collect(res_t res);
.global rescap_collect
rescap_collect:
##################################
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res

    cld         $t0, $zero, 0($c3)
    dli         $t1, res_taken
    bne         $t0, $t1, collect_er        # check this reservation is taken

    cld         $t0, $zero, collection_state($idc)
    bnez        $t0, collect_er             # check we are not already collecting
    dli         $t2, res_collecting

    # We are now collecting. No errors from now on.

    csd         $t1, $zero, collection_state($idc) # Set collection state to 1
    csd         $t2, $zero, 0($c3)                 # Mark reservation as collecting

    cgetbase    $t0, $c3
    cgetoffset  $t1, $c3
    dli         $a0, PHY_MEM_START          #a0 is the start of memory
    dli         $a1, PHY_MEM_END            #a1 the end
    addu        $a2, $t0, $t1               #a2 is the base of our collection
    cld         $t1, $zero, REG_SIZE($c3)
    daddiu      $a4, $t1, RES_META_SIZE     #a4 is the size of collection
    addu        $a3, $a2, $a4               #a3 is the end of our collection (not inclusive)

    csetoffset  $c3, $kdc, $a2
    csetbounds  $c3, $c3, $a4

    # csetcollection $c3 # TODO hypothetical new instruction


    # FIXME we might trample our own caps with the instruction I plan to add. Will have to be careful.
    # FIXME the only one that matters is c6, if we get rescheduled at the position indicated bad things can happen

    subu        $a1, $a1, $a0
    beqz        $a1, collect_loop_end
    csetoffset  $c5, $kdc, $a0
    dli         $t0, CAP_SIZE

collect_loop_start:  # We will now collect every capability with a2 <= base < a3.

    clc         $c6, $zero, 0($c5)          # FIXME <- may get detagged by context switch!
    cbtu        $c6, collect_loop_footer
    cgetbase    $t1, $c6
    sltu        $t2, $t1, $a2
    bnez        $t2, collect_loop_footer
    sltu        $t2, $t1, $a3
    beqz        $t2, collect_loop_footer
                                            # FIXME: although an adversary would be unable to use this race,
                                            # FIXME: we may accidentally overwrite a normal programs data if they reuse
                                            # FIXME: memory in the span of time from when we start collection of this
                                            # FIXME: location to writing back (i.e. it may contain a valid cap later)
    ccleartag   $c6, $c6
    csc         $c6, $zero, 0($c5)

collect_loop_footer:
    subu        $a1, $a1, $t0
    bnez        $a1, collect_loop_start
    cincoffset  $c5, $c5, $t0

collect_loop_end:

    # Make a new reservation here now
    dli         $t0, RES_META_SIZE
    csetoffset  $c3, $kdc, $a2
    csetbounds  $c3, $c3,  $a4
    sub         $a4, $a4, $t0               #some space for metadata

    # cclearderivesfrom $c3 # TODO hypothetical new instruction
    dli         $t1, DEF_DATA_PERMS
    candperm    $c3, $c3, $t1

    cincoffset  $c5, $c3, $t0
    csd         $zero, $zero, 0($c3)        # set state to open
    csetbounds  $c5, $c4, $a4
    cseal       $c3, $c3, $c4               # seal new reservation
    csc         $c5, $zero, 0($c5)          # store the result capability for later

    cclearlo    EN3(c4, c5, c6)
    creturn

collect_er:
    cclearlo    EN4(c3, c4, c5, c6)
    creturn


###################################################
# res_t  rescap_split(capability res, size_t size)
.global rescap_split ##FIXME check size
rescap_split:
###################################################
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res
    cld         $t0, $zero, 0($c3)          # get state
    cld         $t2, $zero, (2 * REG_SIZE)($c3)  # get depth of old res
    clc         $c3, $zero, RES_META_SIZE($c3)   # load return capability from res
    bnez        $t0, split_er               # reservation must be open
    cld         $t0, $zero, (2 * REG_SIZE)($c3) # get pid


    cgetlen     $t1, $c3                    # t1 = original space
    csetoffset  $c5, $c3, $a0               # c5 = start of the new reservation
    csetbounds  $c3, $c3, $a0               # c3 = size for the old reservation
    dli         $t0, RES_META_SIZE
    csc         $c3, $zero, 0($c3)          # store new sized cap for old reservation
    cseal       $c3, $c5, $c4               # this is old reservation but resized
    csd         $zero, $zero, 0($c5)        # set state of new reservation to open
    csd         $t2, $zero, (2 * REG_SIZE)($c5) # set depth for new res
    cincoffset  $c5, $c5, $t0               # new size is reduced by size of metadata in band

    dsubu         $t1, $t1, $t0             # take away space wasted for meta data
    dsubu         $a0, $t1, $a0             # and size for lower address reservation
    csetbounds  $c5, $c5, $a0               # set bounds appropriately
    csc         $c5, $zero, 0($c5)          # store capability in reservation

    cclearlo    EN2(c4, c5)                 # 4 and 5 used as tmps. 3 returns the new reservation
    creturn

split_er:
    cclearlo    EN3(c3, c4, c5)
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
    dli         $t0, res_taken

    cld         $t1, $zero, 0($c3)
    bne         $t0, $t1, merge_er          # check first is taken

    cld         $t1, $zero, 0($c4)
    bne         $t0, $t1, merge_er          # check second is taken

    cld         $t0, $zero, (2 * REG_SIZE)($c3) # get depth of first
    cld         $t1, $zero, (2 * REG_SIZE)($c4) # get depth of second
    bne         $t0, $t1, merge_er          # must have same depth

    cld         $t0, $zero, REG_SIZE($c3)   # get first length
    cld         $t1, $zero, REG_SIZE($c4)   # get second length
    daddiu      $t0, RES_META_SIZE
    cincoffset  $c6, $c3, $t0               # c6 should point to the next reservation
    cne         $t2, $c6, $c4               # if not equal then we are not merging adjacent blocks
    bnez        $t2, merge_er

    addu        $t0, $t0, $t1               # this is the total new length
    li          $t1, res_merged
    csd         $t0, $zero, REG_SIZE($c3)   # store back in the field of the lower address reservation a new length.
    csd         $t1, $zero, 0($c4)          # set higher address reservation to merged
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
    cld         $t0, $zero, 0($c3)          # get state
    bnez        $t0, parent_er              # reservation must be open

    cgetbase    $t0, $c3
    cgetoffset  $t1, $c3
    dli         $a0, RES_META_SIZE
    daddu       $t0, $t0, $t1               # the cursor of the parent is the depth of the child

    clc         $c5, $zero, RES_META_SIZE($c3)   # get capability that ranges over open space
    cgetlen     $t1, $c5                    # get length for parent
    dli         $t2, res_taken
    csd         $t1, $zero, REG_SIZE($c3)   # set length field for parent
    csd         $t2, $zero, 0($c3)          # set state field for parent
    csd         $zero, $zero, 0($c5)        # set state for child
    csd         $t0, $zero, (2*REG_SIZE)($c5) # set depth for child
    cseal       $c3, $c5, $c4               # seal child for return
    dsubu       $t1, $t1, $a0               # steal some space from child
    cincoffset  $c5, $c5, $a0               # capability for space for new reservation
    csetbounds  $c5, $c5, $t1
    csc         $c5, $zero, 0($c5)          # store for future use

    cclearlo    EN2(c4, c5)                 # 4 and 5 used as tmps. 3 returns the new reservation
    creturn
parent_er:

    cclearlo    EN3(c3, c4, c5)
    creturn

nano_kernel_end:
.size nano_kernel, nano_kernel_end - nano_kernel_start

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

    cld         $t1, $t0, REG_SIZE($idc)                # $t1 = len of record
    daddiu      $t1, $t1, -1
    bnez        $t1, create_ptable_er                   # $t1 must be 1

    clw         $t1, $t0, 0($idc)                       # $t1 = state
    beqz        $t1, 1f                                 # state must be un-used
    dli         $t7, page_ptable_free                   # or can safely re-use old ptables for new ptables
    bne         $t1, $t7, create_ptable_er

1:
    dli         $t1, page_ptable
    csw         $t1, $t0, 0($idc)                       # set to nano owned

    dsll        $t0, $a0, PHY_PAGE_SIZE_BITS
    dli         $t1, PHY_MEM_START
    daddu       $t0, $t0, $t1                          # t0 is the address of the new page
    dsll        $t1, $a1, PAGE_TABLE_ENT_BITS          # t1 is index in the parent page to write to

    # Check table not already created/deleted
    cld         $t3, $t1, 0($c3)
    bnez        $t3, create_ptable_er
    nop

    csd         $t0, $t1, 0($c3)
    dli         $t1, PAGE_TABLE_SIZE
    csetoffset  $c3, $kdc, $t0
    csetbounds  $c3, $c3, $t1
    csetoffset  $c4, $kdc, $t2
    cseal       $c3, $c3, $c4

    cmove       $c4, $c17
    jal         nano_zero_page
    nop

    cmove       $c17, $c4
    cclearlo    EN2(c4,c17)
    creturn

create_ptable_er:
    cclearlo    EN2(c3,c4)
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

    cld         $t1, $t0, REG_SIZE($idc)                # $t1 = len of record
    daddiu      $t1, $t1, -2
    bnez        $t1, create_mapping_er                  # $t1 must be 2

    clw         $t1, $t0, 0($idc)                       # $t1 is the state of this phy page
    bnez        $t1, create_mapping_er                  # must be un-used
    dsll        $t2, $a1, PAGE_TABLE_ENT_BITS           # t2 is offset of page entry to c3

    cld         $t1, $t2, 0($c3)                        # check -virtual- mapping is un-used
    bnez        $t1, create_mapping_er

    dli         $t1, page_mapped
    csw         $t1, $t0, 0($idc)                       # set page to be mapped

    # TODO add perms
    # TODO add autharising cap
    # TODO add the checked top bits

    dsll        $t1, $a0, PFN_SHIFT
    or          $t1, $t1, $a2
    csd         $t1, $t2, 0($c3)                        # store EntryLo
create_mapping_er:
    cclearlo    EN2(c3,c4)
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
    daddiu      $t1, $t1, (VTABLE_LEVELS-1)             # t1 zero if last level
    beqz        $t2, free_er

    csetoffset  $c4, $kdc, $t0

    sltiu       $t0, $a0, PAGE_TABLE_ENT_PER_TABLE
    beq         $t0, $zero, free_er                     # $a0 >= PAGE_TABLE_ENT_PER_TABLE

    cunseal     $c3, $c3, $c4
    dsll        $t2, $a1, PAGE_TABLE_ENT_BITS           # t2 is offset of page entry to c3

    cld         $t3, $t2, 0($c3)                        # t3 is the PFN/Physical adress of page
    beqz        $t3, free_er
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

    csd         $t0, $t2, 0($c3)                        # store -1 in vtable to mean 'used'
    csw         $a3, $t1, REG_SIZE($idc)                # store to update phy page status

    # FIXME need to invalidate TLB so old mappings go away
free_er:
    cclearlo    EN2(c3, c4)
    creturn

#############################################################################################
# void clear_mapping(ptable_t table, register_t vaddr, res_t reservation)
.global clear_mapping
clear_mapping:
#############################################################################################






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

    dsll        $k0, $k0, CHECKED_BITS                                              # clear top bits
    dsrl        $k0, $k0, (CHECKED_BITS + L1_BITS + L2_BITS + UNTRANSLATED_BITS)    # clear lower bits
    dsll        $k0, $k0, PAGE_TABLE_ENT_BITS                                       # scale
    daddu       $k1, $k1, $k0

    cld         $k1, $k1, 0($kdc)                                   # k1 = phy pointer to L1
    slti        $k0, $k1, -1
    beqz        $k0, tlb_table_missing

    dmfc0       $k0, cp0_badvaddr
    dsll        $k0, $k0, CHECKED_BITS + L0_BITS                                   # clear top bits
    dsrl        $k0, $k0, (CHECKED_BITS + L0_BITS + L2_BITS + UNTRANSLATED_BITS)   # clear lower bits
    dsll        $k0, $k0, PAGE_TABLE_ENT_BITS                                      # scale
    daddu       $k1, $k1, $k0

    cld         $k1, $k1, 0($kdc)                                   # k1 = phy pointer to L2
    slti        $k0, $k1, -1
    beqz        $k0, tlb_table_missing

    dmfc0       $k0, cp0_badvaddr
    dsll        $k0, $k0, CHECKED_BITS + L1_BITS + L0_BITS                         # clear top bits
    dsrl        $k0, $k0, (CHECKED_BITS + L1_BITS + L0_BITS + UNTRANSLATED_BITS)   # clear lower bits
    dsll        $k0, $k0, PAGE_TABLE_ENT_BITS                                      # scale
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
