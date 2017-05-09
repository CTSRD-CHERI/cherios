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

# We will lay out our contexts like this

# struct context_t {
#   reg_frame_t
#   enum state{allocated, dead} (size of a cap)
#};

.set CONTEXT_SIZE, CHERI_FRAME_SIZE + CAP_SIZE


.set N_CONTEXTS,         64             # We never reallocate these, so its currently a huge limitation
#FIXME we need to choose appropriate types and remove their accessibility from the rest of the system
.set CONTEXT_TYPE,       0x5555         # The type of contexts
.set NANO_KERNEL_TYPE,   0x6666         # The type of sealed local data
.set RES_TYPE,           0x7777         # The type that means 'is allowed to
.set RES_VIEW_TYPE,      0x7778         # The type that means 'is allowed to

# Out virtual state is harder. We will ask the user to allocate this for us, using our allowance mechanism
# struct virt_ent {
    #PFN1
    #PFN2
    #enum state{un-used, allocated, stale}
#};



.set VPN_MASK, ((1 << 64) - 1) ^ ((1 << 13) - 1)
.set PFN_MASK, ((1 << 55) - 1) ^ ((1 << 6) - 1)

#define cp0_index     $0
#define cp0_entryhi   $10
#define cp0_entrylo0  $2
#define cp0_entrylo1  $3
#define cp0_pagemask  $5
#define cp0_random    $1
#define cp0_wired     $6
#define cp0_context   $4


#struct res_t {
#    enum state{open, taken, merged, collecting}
#    size_t length
#    size_t pid
#    padding to size of one cap
#    union{cap for region, user data}
#
#}


# This is the physical memory we will scan in order to support revocation
# TODO use the proper physical window:
.set PHY_MEM_START, 0x9000000000000000
.set PHY_MEM_SIZE,  (1 << 32)
.set PHY_MEM_END,   PHY_MEM_START + PHY_MEM_START
#.set NANO_SIZE, __nano_size
.set NANO_SIZE, 0x4000000
.set PHY_PAGE_SIZE_BITS, 12
.set PHY_PAGE_SIZE, (1 << PHY_PAGE_SIZE_BITS)
.set N_PHY_PAGES_ENTRIES, PHY_MEM_SIZE / PHY_PAGE_SIZE
.set PHY_PAGE_ENTRY_SIZE, 2 * REG_SIZE
.set PHY_PAGE_ENTRY_SIZE_BITS, REG_SIZE_BITS + 1


# We will lay out PHYSICAL state like this. The PFN is just the index in the table:
# struct phy_ent {
#    register_t state {un-used, nano_owned, system_owned, mapped}
#    register_t VPN2        This is the VPN field. Once set cannot be changed, unless we revoke the VPN.
#
#};

.set PHY_UNUSED, 0
.set PHY_NANO, 1
.set PHY_SYSTEM, 2
.set PHY_MAPPED, 3


# The general idea is this: Physical pages are un-used and their state can be set (once) by the system
# Some will be set to nano_owned for private use (constant set at init time).
# Some will system_owned, we hand out PHYSICAL capabilities
# Some will be mapped, and we will remember the mapping. When the VPN is proveably not in use, we can go back to un-used

####################################################################################################################
# The (very incorrectly named) global data for the nano kernel. idc will hold a capability that covers the range
# locals_start to locals_end.
####################################################################################################################

START_LOCALS CAP_SIZE_BITS

# Stuff to do with context switch #
local_cap_var current_context
local_cap_var exception_context
local_cap_var next_context
local_reg_var exception_level
local_reg_var exception_cause

# Stuff to do with virtual space management
local_reg_var collection_state

# Method table for nano kernel
local_cap_var create_context_cap
local_cap_var destroy_context_cap
local_cap_var context_switch_cap
local_cap_var critical_section_enter_cap
local_cap_var critical_section_exit_cap
local_cap_var set_exception_handler_cap
local_cap_var unlock_context_cap
local_cap_var tlb_write_cap
local_cap_var rescap_take_cap
local_cap_var rescap_info_cap
local_cap_var rescap_collect_cap
local_cap_var rescap_split_cap
local_cap_var rescap_merge_cap
local_cap_var rescap_parent_cap
local_cap_var get_phy_page_cap
.set cap_table_end, local_ctr

local_var context_table, N_CONTEXTS * CONTEXT_SIZE, CAP_SIZE_BITS

local_var page_table, N_PHY_PAGES_ENTRIES * PHY_PAGE_ENTRY_SIZE, REG_SIZE_BITS
END_LOCALS

.text
.section .init


.set DEF_DATA_PERMS, (Perm_All & ~(Perm_Access_System_Registers | Perm_Seal))
.set DEF_SEALING_PERMS, Perm_Seal

nano_kernel_start:

#################################################################
# nano_kernel_init(register_t unmanaged_space, register_t return_addr, register_t arg0)
.global nano_kernel_init
nano_kernel_init:
################################################################
	# Populate exception registers: $kdc and $kcc
	cgetpccsetoffset $kcc, $zero                 # kdcc will hold the code global capability
	cgetdefault	$kdc                                    # kdc will hold the data global capability

    dla         $k0, locals_start
    dli         $k1, locals_size
    csetoffset  $kr1c, $kdc, $k0
    csetbounds  $kr1c, $kr1c, $k1                       # kr1c will hold a capability to our locals

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

    csetbounds  $kr2c, $kr2c, $zero
    csc         $kr2c, $zero, exception_context($kr1c)   # Set exception context = NULL

    csd         $zero, $zero, exception_level($kr1c)     # Set exception level to 0
    csd         $zero, $zero, exception_cause($kr1c)     # Set exception cause to 0

    # NOTE: context_table does not need initialising. This is done as we allocate new contexts.

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
    init_table create_context
    init_table destroy_context
    init_table context_switch
    init_table critical_section_enter
    init_table critical_section_exit
    init_table set_exception_handler
    init_table unlock_context
    init_table tlb_write
    init_table rescap_take
    init_table rescap_info
    init_table rescap_collect
    init_table rescap_split
    init_table rescap_merge
    init_table rescap_parent
    init_table get_phy_page

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
    slt         $t0, $a0, $t0
    beq         $t0, $zero, nano_kernel_die
    nop

    # TODO we need to set NANO_SIZE of physical pages to nano owned. Then a0 pages to system owned. The rest are free

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
    move        $a1, $a3
    cjr         $c17
    move        $a0, $a2



##########################################################################
# idc/kdc will provide us a capability to our locals in everything below #
##########################################################################

.text

.global make_first_reservation     # FIXME this is handing out reserved caps to PHYSICAL space. It should be virtual.
make_first_reservation: #FIXME dumbly copied out of init. It has been seperated as it can only be created when virtual
                        #FIXME is supported
    # TODO we should actually collect the initial reservation in case the boot loader made a mistake

    csetoffset  $c13, $kdc, $t2
    candperm    $c13, $c13, $k0
    csd         $zero, $zero, 0($c13)                   # set state to open
    csd         $zero, $zero, (2 * REG_SIZE)($c13)      # set parent to 0
    dli         $k1, RES_TYPE
    csetoffset  $c14, $kdc, $k1
    cseal       $c4, $c13, $c14                         # seal reservation
    dli         $k1, CAP_SIZE
    cincoffset  $c13, $c13, $k1

    csetbounds  $c13, $c13, $t3
    csc         $c13, $zero, 0($c13)                    # store cap to rest of reversed mem for use later
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
    cseal   $kr2c, $c3, $kr1c                           # Seal c3 to pass to exception handler
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
    beqz       $k0, normal_return
    nop

exceptional_return:
    move       $a0, $k0                                 # pass the cause to the exception handler
    cmove      $c3, $kr2c                               # pass the sealed context to the exception handler
    dmfc0      $k0, $12                                 # disable interrupts otherwise the exception handlel
    dli        $k1, 1                                   # will may immediately have one
    not        $k1, $k1
    and        $k0, $k0, $k1
    mtc0       $k0, $12
normal_return:
    cgetoffset $k0, $epcc
    dmtc0      $k0, $14
    eret


#############################################
# capability get_phy_page(register_t page_n)
.global get_phy_page
get_phy_page:
#############################################


    dli         $t0, N_PHY_PAGES_ENTRIES
    slt         $t0, $a0, $t0
    beq         $t0, $zero, get_phy_page_end            # $a0 >= $t0
    sll         $t0, $a0, PHY_PAGE_ENTRY_SIZE_BITS
    dli         $t1, page_table
    daddu       $t0, $t0, $t1                           # $t0 is the offset of our phy entry
    cld         $t1, $t0, 0($idc)                       # $t1 = state
    bnez        $t1, get_phy_page_end                   # state must be un-used
    dli         $t1, PHY_SYSTEM
    csd         $t1, $t0, 0($idc)                       # set to system owned

    dli         $t1, PHY_MEM_START
    sll         $t2, $a0, PHY_PAGE_SIZE_BITS
    dli         $t0, PHY_PAGE_SIZE
    daddu       $t1, $t1, $t2                           # t1 is the offset required for our cap
    csetoffset  $c3, $kdc, $t1
    csetbounds  $c3, $c3, $t0                           # bounds should be one page
    dli         $t0, DEF_DATA_PERMS   #t0 is the permission mask
    candperm    $c3, $c3, $t0

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





######################################################################################################
# reg_frame_t* unlock_context(context_t context); # Dont care about leaking here, its for debug only
.global unlock_context
unlock_context:
######################################################################################################
    dli         $t0, CONTEXT_TYPE
    csetoffset  $c4, $kdc, $t0
    cunseal     $c3, $c3, $c4
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


#################################################################################################
# int tlb_write(register_t EntryHi, register_t EntryLo0, register_t EntryLo1, register_t index))
.global tlb_write #TODO this is very WIP
tlb_write:
#################################################################################################

#TODO check this does not break our invarient

    mtc0 $a0, cp0_entryhi
    mtc0 $a1, cp0_entrylo0
    mtc0 $a2, cp0_entrylo1

    daddiu  $t0, $a3, 1
    bnez    $t0, indexed
    nop

random:
    ehb
    tlbwr
    j end
    nop

indexed:
    mtc0 $a3, cp0_index
    ehb
    tlbwi

end:
    creturn


####################################
# capability rescap_take(res_t res)
.global rescap_take
rescap_take:
####################################
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res
    cld         $t0, $zero, 0($c3)          # get state
    csetbounds  $c4, $c4, $zero             # clear our sealing cap
    bnez        $t0, take_er                # error if not open
    dli         $t0, 1

    cgetlen     $t1, $c3
    csd         $t0, $zero, 0($c3)          # set to taken
    csd         $t1, $zero, REG_SIZE($c3)   # set length (needed for merge)
    clc         $c3, $zero, CAP_SIZE($c3)   # load return capability from struct
    creturn
take_er:
    csetbounds  $c3, $c3, $zero
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
    clc         $c3, $zero, CAP_SIZE($c3)   # load return capability from struct
    dli         $t1, RES_VIEW_TYPE
    csetoffset  $c4, $kdc, $t1              # create sealing cap
    cseal       $c3, $c3, $c4
    bnez        $t0, info_er                # error if not open
    csetbounds  $c4, $c4, $zero             # clear our sealing cap

    creturn
info_er:
    csetbounds  $c3, $c3, $zero
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
    dli         $t1, 1
    bne         $t0, $t1, collect_er        # check this reservation is taken

    cld         $t0, $zero, collection_state($idc)
    bnez        $t0, collect_er             # check we are not already collecting
    dli         $t2, 3

    # We are now collecting. No errors from now on.

    csd         $t1, $zero, collection_state($idc) # Set collection state to 1
    csd         $t2, $zero, 0($c3)                 # Mark reservation as collecting

    cgetbase    $t0, $c3
    cgetoffset  $t1, $c3
    dli         $a0, PHY_MEM_START          #a0 is the start of memory
    dli         $a1, PHY_MEM_END            #a1 the end
    addu        $a2, $t0, $t1               #a2 is the base of our collection
    cld         $t1, $zero, REG_SIZE($c3)
    daddiu      $a4, $t1, CAP_SIZE          #a4 is the size of collection
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
    dli         $t0, CAP_SIZE
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
.global rescap_split
rescap_split:
###################################################
    dli         $t0, RES_TYPE
    csetoffset  $c4, $kdc, $t0              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res
    cld         $t0, $zero, 0($c3)          # get state
    cld         $t2, $zero, (2 * REG_SIZE)($c3)  # get depth of old res
    clc         $c3, $zero, CAP_SIZE($c3)   # load return capability from res
    bnez        $t0, split_er               # reservation must be open
    cld         $t0, $zero, (2 * REG_SIZE)($c3) # get pid


    cgetlen     $t1, $c3                    # t1 = original space
    csetoffset  $c5, $c3, $a0               # c5 = start of the new reservation
    csetbounds  $c3, $c3, $a0               # c3 = size for the old reservation
    dli         $t0, CAP_SIZE
    csc         $c3, $zero, 0($c3)          # store new sized cap for old reservation
    cseal       $c3, $c5, $c4               # this is old reservation but resized
    csd         $zero, $zero, 0($c5)        # set state of new reservation to open
    csd         $t2, $zero, (2 * REG_SIZE)($c5) # set depth for new res
    cincoffset  $c5, $c5, $t0               # new size is reduced by size of one cap to store metadata in band

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
    dli         $t0, 1

    cld         $t1, $zero, 0($c3)
    bne         $t0, $t1, merge_er          # check first is taken

    cld         $t1, $zero, 0($c4)
    bne         $t0, $t1, merge_er          # check second is taken

    cld         $t0, $zero, (2 * REG_SIZE)($c3) # get depth of first
    cld         $t1, $zero, (2 * REG_SIZE)($c4) # get depth of second
    bne         $t0, $t1, merge_er          # must have same depth

    cld         $t0, $zero, REG_SIZE($c3)   # get first length
    cld         $t1, $zero, REG_SIZE($c4)   # get second length
    daddiu      $t0, CAP_SIZE
    cincoffset  $c6, $c3, $t0               # c6 should point to the next reservation
    cne         $t2, $c6, $c4               # if not equal then we are not merging adjacent blocks
    bnez        $t2, merge_er

    addu        $t0, $t0, $t1               # this is the total new length
    li          $t1, 2
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
    dli         $a0, CAPS_SIZE
    daddu       $t0, $t0, $t1               # the cursor of the parent is the depth of the child

    clc         $c5, $zero, CAP_SIZE($c3)   # get capability that ranges over open space
    cgetlen     $t1, $c5                    # get length for parent
    dli         $t2, 1
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
        dla $k1, locals_start
        csetoffset $kr1c, $kdc, $k1
        cld $k1, $zero, exception_level($kr1c)
        beqz $k1, take_exception
        nop
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
		dla	$k1, context_switch_exception_entry
		jr	$k1
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
    nop

tlb_trampoline_end:
    .global tlb_trampoline_end
    .end tlb_trampoline
    .size tlb_trampoline, tlb_trampoline_end - tlb_trampoline
