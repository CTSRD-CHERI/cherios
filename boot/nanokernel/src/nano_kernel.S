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


# We will lay out our contexts like this

# struct context_t {
#   reg_frame_t
#   enum state{allocated, dead} #TODO currently only set allocated
#};

.set CONTEXT_SIZE, CHERI_FRAME_SIZE + CAP_SIZE


.set N_CONTEXTS,         64             # We never reallocate these, so its currently a huge limitation
#FIXME we need to choose appropriate types and remove their accessibility from the rest of the system
.set CONTEXT_TYPE,       0x5555         # The type of contexts
.set NANO_KERNEL_TYPE,   0x6666         # The type of sealed local data

####################################################################################################################
# The (very incorrectly named) global data for the nano kernel. idc will hold a capability that covers the range
# locals_start to locals_end.
####################################################################################################################

START_LOCALS CAP_SIZE_BITS

local_cap_var global_cap
local_cap_var sealing_cap
local_cap_var current_context
local_cap_var exception_context
local_cap_var next_context
local_reg_var exception_level
local_reg_var exception_cause

local_cap_var create_context_cap
local_cap_var destroy_context_cap
local_cap_var context_switch_cap
local_cap_var critical_section_enter_cap
local_cap_var critical_section_exit_cap
local_cap_var set_exception_handler_cap
local_cap_var unlock_context_cap
.set cap_table_end, local_ctr

local_var context_table, N_CONTEXTS * CONTEXT_SIZE, CAP_SIZE_BITS

END_LOCALS

.text

nano_kernel_start:

#########################
.global nano_kernel_init
nano_kernel_init:
#########################
    li          $zero, next_context
	# Populate exception registers: $kdc and $kcc
	cgetpcc		$kcc                                    # kdcc will hold the code global capability
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
    csc         $kr2c, $zero, sealing_cap($kr1c)         # Create and store sealing cap for contexts

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
    csc         $c13, $zero, \name\()_cap($kr1c)
.endm

    # Store a sealed capability for each nanokernel function in a table
    init_table create_context
    init_table destroy_context
    init_table context_switch
    init_table critical_section_enter
    init_table critical_section_exit
    init_table set_exception_handler
    init_table unlock_context

    # Pass a read-only capability to the cap table
    dli          $k0, create_context_cap
    dli          $k1, cap_table_end - create_context_cap
    csetoffset   $c1, $kr1c, $k0
    csetbounds   $c1, $c1, $k1
    dli          $k0, (Perm_Load | Perm_Load_Capability)
    candperm     $c1, $c1, $k0


    cseal       $c2, $kr1c, $kr2c                        # Pass a sealed capability to our locals

    # Now remove capabilities to the nano kernel. We should deny access to kernel regs
    # TODO: and the tlb and our own code/data and certain sealing types
    # TODO: and writing exception vectors

    dli        $k0, (Perm_All & ~(Perm_Access_System_Registers))
    #candperm   $kdc, $kdc, $k0
    #candperm   $c17,  $c17,  $k0
    cclearlo    (1 << 13)                               # Used $c13 as a tmp.
    csetdefault $kdc
    cjr         $c17
    nop




##########################################################################
# idc/kdc will provide us a capability to our locals in everything below #
##########################################################################


#####################################################################
# void context_switch(context_t restore_from, context_t*  store_to);
.global context_switch
context_switch:
#####################################################################

    # Enter an exception level to turn off interrupts. We must at the least switch from restore_from.
    # However we might switch to the exception_context

    dmfc0   $k0, $12
    ori     $k0, 2                                      # set SR(EXL)
    mtc0    $k0, $12

    dli     $k0, 0
    # FIXME check $c3/$c4 so this cant throw an exception.

context_switch_local_entry:                             # void context_switch_local_entry(reg_t cause)
    # We cant use $idc fo a bit as we may enter here from an exception, so we put it in $kr1c for now
    cmove   $kr1c, $idc
    # TODO unseal these
    cmove   $idc, $c18
    cmove   $epcc, $c17

context_switch_exception_entry:                         # kr1c should contain a good pointer to our locals
    clc     $kr2c, $zero, current_context($kr1c)

    save_reg_frame_idc $kr2c, $k1, $c1, $epcc, $idc     # Save state

    # Now we can undo our weird juggle and use $idc again
    cmove   $idc, $kr1c

    bnez    $k0, switch_exception                       # set if we used the local entry
    clc      $kr1c, $zero, sealing_cap($idc)            # meant to be in delay

    cunseal  $c3, $c3, $kr1c

    # Save a context to
    cseal    $kr2c, $kr2c, $kr1c                        # seal old context
    csc      $kr2c, $zero, 0($c4)                       # save it in store_to


switch_restore:                                         # void switch_restore(context_t restore_from)

    csc      $c3, $zero, current_context($idc)          # set c3 it as the current context

    # Check if we had supressed any interrupts. If we did, we can pretty much treat this like the
    # Beginning of an exception and just restore the exception context

    cld     $a0,   $zero, exception_cause($idc)         # load cause
    beqz    $a0,   switch_restore_final                 # no exception
    csd     $zero, $zero, exception_level($idc)         # critical_state.level = 0

switch_exception:                                       # void switch_exception(context_t victim, reg_t cause)
    mtc0    $a0, $13                                    # Spoof cause
    li      $k0, 1
    csd     $zero, $zero, exception_cause($idc)
                # If we were doing it properly we would modify it to have the faulting instruction be from the new
                # activation. However, for now as only the bits to check the type of
                # interrupt we will just use this.

    clc     $c3, $zero, current_context($idc)           # Current context is the victim
    cseal   $c4, $c3, $kr1c                             # Seal c3 to pass to exception handler
    clc     $c3, $zero, exception_context($idc)         # load exception context
    csc     $c3, $zero, current_context($idc)           # set it as current context
    csc     $c4, $zero, FRAME_C3_OFFSET($c3)                  # pass the sealed context to the exception handler

    # Restore everything, we dont have a register spare for $c0 so set default while restoring
    # We use exception registers here. These are not used by the critical section check in exception.S

    .macro crestore_setc0 greg, offset, frame
        crestore \greg, \offset, \frame
        .if \offset == 0
            csetdefault \greg
        .endif
    .endm

switch_restore_final:

    # c0 is not really stored in $c1, we just use it as temporary. $c3 will be overwritten so use $kr1c.
    cmove $kr1c, $c3
    restore_reg_frame_gen crestore_setc0, grestore, $kr1c, $t0, $c1, $epcc

return:
    bnez       $k0, exceptional_return
    nop

    cgetoffset $k0, $epcc
    dmtc0      $k0, $14
    eret

exceptional_return:
    #TODO we should drop the bit to allow access to k regs, but still be in an exception level
    cjr        $epcc
nop



########################################################
# context_t create_context(reg_frame_t* initial_state, int pass_ref);
.global create_context
create_context:
########################################################

    clc         $c13, $zero, sealing_cap($idc)              # Load sealing capability
    cmove       $c4,  $c3
    move        $t3,  $a0                                   # TODO remove this when possible
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

    cseal       $c4, $c3, $c13                              # Return sealed cap

    beqz        $t3, create_context_end
    nop
    csc         $c4, $zero, FRAME_C3_OFFSET($c3)            # TODO remove this when possible


create_context_end:
    cmove       $c3, $c4
    cclearlo    (1 << 4) | (1 << 5) | (1<< 12) | (1 << 13) | (1 << 14) | (1<<15)
                                                            # We used c4,c12,c13,c14,c15. c3 is return.
    creturn




########################################################################
# context_t destroy_context(context_t context, context_t restore_from);
.global destroy_context
destroy_context: #TODO ALL
########################################################################

    clc         $c13, $zero, sealing_cap($idc)              # Load sealing capability
    cunseal     $c3, $c3, $c13                              # unseal context we are destroying
    clc         $c14, $zero, current_context($idc)          # load current context
    ceq         $k0, $c3, $c14                              # k0 = 1 if we are deleting ourselves
    dli          $k1, 1
    dli          $t0, CHERI_FRAME_SIZE
    beqz        $k0, destroy_context_end
    csd         $k1, $t0, 0($c3)                            # set state to dead (in delay slot)

    # If we are here we are destroying ourselves, thus we should restore restore_from
    cunseal     $c3, $c4, $c13
    dmfc0       $k0, $12
    ori         $k0, 2                                      # set SR(EXL)
    mtc0        $k0, $12
    j           switch_restore

destroy_context_end:
    cclearlo    (1 << 13) | (1 << 14) | (1 << 3)
    creturn





######################################################################################################
# reg_frame_t* unlock_context(context_t context); # Dont care about leaking here, its for debug only
.global unlock_context
unlock_context:
######################################################################################################

    clc         $c4, $zero, sealing_cap($idc)
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

    cld        $a0, $zero, exception_cause($idc)
    beqz       $a0, kernel_critical_section_exit_end

    dmfc0      $k0, $12
    ori        $k0, 3                                       # set SR(EXL) and SR(IE)
    mtc0       $k0, $12
    dli        $k0, 1
    j          context_switch_local_entry                   # calling context switch with k0 = 1 will
                                                            # will switch to the exception context

kernel_critical_section_exit_end:
    creturn




#################################################
# void set_exception_handler(context_t context);
.global set_exception_handler
set_exception_handler:
#################################################
    clc         $c13, $zero, sealing_cap($idc)
    cunseal     $c13, $c3, $c13
    csc         $c13, $zero, exception_context($idc)
    creturn




###########################################################################################################
# Relocatable exception vector; checks we are not in a soft disable and then jumps to the context switcher
# normal program memory.  This runs with KCC installed in PCC.
###########################################################################################################

		.global kernel_exception_trampoline
		.ent kernel_exception_trampoline
kernel_exception_trampoline:
# cancel if we are in critical section.
# We assume that critical code never throws exceptions and only async interrupts happen.
# TODO: switch happens in an exception level, but other bits of the nano kernel may need thinking about
        dla $k1, locals_start
        csetoffset $kr1c, $kdc, $k1
        cld $k1, $zero, exception_level($kr1c)
        beqz $k1, take_exception
        nop
skip_exception:
        dmfc0   $k1, $13
        csd     $k1, $zero, exception_cause($kr1c)
        dmfc0   $k1, $12
        daddiu  $k1, -1     # As interrupts had to be enabled #TODO learn how to program and do &~1. d'oh.
        mtc0    $k1, $12    # Disable interrupts, we are happy to take the hit as this is rare
        eret
take_exception:
		dla	$k1, context_switch_exception_entry
		jr	$k1
		li  $k0, 1
kernel_exception_trampoline_end:
		nop
		.global kernel_exception_trampoline_end
		.end kernel_exception_trampoline
		.size kernel_exception_trampoline, kernel_exception_trampoline_end - kernel_exception_trampoline
		.size kernel_exception_trampoline_end, 1 # 0 triggers capsizefix warning

nano_kernel_end:
.size nano_kernel, nano_kernel_end - nano_kernel_start