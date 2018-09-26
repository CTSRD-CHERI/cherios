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
#include "mips.h"
#include "asm.S"
#include "assembly_utils.h"
#include "nano/nanokernel.h"
#include "dylink.h"

# FIXME. SPecial registers are now no longer general purpose. This makes things work:
.set cheri_sysregs_accessible

.set CACHE_LINE_SIZE,                64

# Top bits of virtual address decide which range we are using
# FIXME these are from see mips run and are probably wrong. Use versions in mips.h
.set PHY_MEM_START_TOP,             0x80
.set PHY_MEM_START_UNCACHED_TOP,    ((MIPS_XKPHYS_UNCACHED << 3) | PHY_MEM_START_TOP)
.set PHY_MEM_START_CACHED_TOP,      ((MIPS_XKPHYS_CACHED_NC << 3) | PHY_MEM_START_TOP)
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
.set PHY_MEM_START_CACHED,          (PHY_MEM_START_CACHED_TOP << TOP_ADDR_SHIFT)
.set PHY_MEM_START_UNCACHED,        (PHY_MEM_START_UNCACHED_TOP << TOP_ADDR_SHIFT)

.set PHY_MEM_END_CACHED,            PHY_MEM_START_CACHED + PHY_RAM_SIZE + IO_HOLE

#.set NANO_SIZE, __nano_size
.set NANO_SIZE, 0x4000000

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

.data

# We would like to bound these exactly. Help with a few bits
.p2align 8

START_LOCALS CAP_SIZE_BITS

# Stuff to do with context switch. Allocated for each smp core #

#ifdef SMP_ENABLED
local_array_start context_array, CAP_SIZE_BITS, SMP_CORES
#endif

local_cap_var current_context
local_cap_var ex_pcc_dummy          # Set to null. This is here so we match the CTLP requirements for user exceptions
local_cap_var K_CAP                 # used by sha256 WARN: DONT MOVE, I HAVE HARDCODED THIS IN sha256.S (sorry)
local_cap_var exception_context
local_cap_var victim_context
local_cap_var switch_cap_tmp
local_reg_var exception_level
local_reg_var last_exception_cause
local_reg_var last_exception_ccause
local_reg_var last_bad_vaddr
local_reg_var since_switched
local_reg_var since_shotdown
local_reg_var in_switch
local_reg_var supressed
local_reg_var unested_cause
#ifdef SMP_ENABLED
local_array_end context_array, CAP_SIZE_BITS,  SMP_CORES
#endif

# For context allocation
local_cap_var next_context

# Stuff to do with virtual space management
local_reg_var made_first_res
local_reg_var revoke_state
local_reg_var revoke_base
local_reg_var revoke_bound
local_reg_var need_shootdown

# A list of masks for which hardware register fields can be accessed
.set register_mask_table_size, 0
#define CALC_SIZE(...) .set register_mask_table_size, register_mask_table_size+1;
NANO_REG_LIST(CALC_SIZE)
#define EMIT_DATA(name, num, selector, mask, ...) .word mask;
local_init_var register_mask_table, (4 * register_mask_table_size), 2
NANO_REG_LIST(EMIT_DATA)
#undef CALC_SIZE
#undef EMIT_DATA

# Method table for nano kernel
NANO_KERNEL_IF_LIST(LOCAL_CAP_VAR_MACRO)
.set cap_table_end, local_ctr

local_var top_virt_page, PAGE_TABLE_SIZE, PAGE_TABLE_ENT_BITS

local_var tres_bitfield, TRES_BITFIELD_SIZE, 1

local_var context_table, N_CONTEXTS * CONTEXT_SIZE, CAP_SIZE_BITS

local_var phy_page_table, (TOTAL_PHY_PAGES + 1) * PHY_PAGE_ENTRY_SIZE, 8

local_align 8

END_LOCALS

.text
.section .init

.set DEF_DATA_PERMS, (Perm_All & ~(Perm_Access_System_Registers | Perm_Seal | Perm_Unseal))
.set DEF_SEALING_PERMS, (Perm_Seal | Perm_Unseal)

nano_kernel_start:

# We have no stack, and this sub routine may need to be callable from a couple of places. Set appropriately.

.set zero_page_tmp_1,   $t8
.set zero_page_ctmp_1,  $cnull
.set zero_page_arg,     $t0

.set UNROLL_FACTOR,     8

# Zeros page. I unrolled this a few times on point of principle. This is in no way optimal.

nano_zero_page:
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
    nop

#ifdef SMP_ENABLED

# t0 will contain our id
nano_smp_trap:
# idc for global state
    cmove       $idc, $kr1c

# kr1c for thread local
    dli         $t1, context_array_size
    multu       $t0, $t1
    mflo        $t2
    cincoffset  $kr1c, $kr1c, $t2
    dli         $t1, context_array_size
    csetbounds  $kr1c, $kr1c, $t1

# wait for signal
nano_smp_spin:
    clc         $c3, $zero, current_context($kr1c)
    YIELD
    cbtu        $c3, nano_smp_spin
    nop

    dmfc0       $t0, $12
    dli         $t1, ~(MIPS_CP0_STATUS_BEV)
    and         $t0, $t0, $t1
    dmtc0       $t0, $12

    clc         $c3, $zero, exception_context($kr1c)        # load exception context
# setting k0 to -1 to make fake exception
    b           switch_restore_final
    dli         $k0, -1
#endif


# Notes on calling rescap_take: provide c4 as null to avoid write. Will return in c3 and c4. Will use c5 #
# Subroutine will clober c3,c4,c5,c14,c15,t*,a7. c3_in=res.c3_out=code.c4_out=data #

.macro SUBROUTINE_TAKE
    cmove       $c14, $c17                          # save return code
    cmove       $c15, $c18                          # save return data

    dla         $t0, rescap_take
    cgetpccsetoffset $c12, $t0
    cjalr       $c12, $c17                          # call rescap_take
    cnull       $c4                                 # null c4 for call to rescap_take

    cmove       $c17, $c14                          # restore return code
    cmove       $c18, $c15                          # restore return data
.endm

.macro SET_EXL tmp
    #di <-- FIXME we really need this insutrction here for atomicity...
    mfc0    \tmp, MIPS_CP0_REG_STATUS
    ori     \tmp, 3                                      # set SR(EXL) and SR(IE)
    mtc0    \tmp, MIPS_CP0_REG_STATUS
.endm

.macro QEMU_BUG_FIX tmp
    cgetoffset \tmp, $epcc
    dmtc0      \tmp, MIPS_CP0_REG_EPC
.endm

#ifdef SMP_ENABLED
############################################################### (safe)
# int smp_context_start(context_t start_as, register_t cpu_id)
.global smp_context_start
smp_context_start:
###############################################################

    dli         $v0, -1
    sltiu       $t1, $a0, SMP_CORES
    beqz        $t1, smp_context_start_er

    dli         $t0, CONTEXT_TYPE
    csetoffset  $c4, $kdc, $t0
    cunseal     $c3, $c3, $c4

    dli         $t0, context_array_size
    multu       $t0, $a0
    mflo        $t0
    daddiu      $t0, current_context

    cincoffset  $c4, $idc, $t0               # current_context for smp core with cpu_id of $a0

smp_signal_loop:
    cllc        $c5, $c4
    cbts        $c5, smp_context_start_er
    nop
    cscc        $t0, $c3, $c4
    beqz        $t0, smp_signal_loop
    dli         $v0, 1

smp_context_start_er:
    cclearlo    EN3(c3, c4, c5)
    CRETURN
#endif

################################################################# (TODO unsafe)
# nano_kernel_init(register_t unmanaged_space, register_t return_addr, packaged args)
.global nano_kernel_init
nano_kernel_init:
################################################################
	# Populate exception registers: $kdc and $kcc
	cgetpccsetoffset $kcc, $zero                        # kdcc will hold the code global capability
	cgetdefault	$kdc                                    # kdc will hold the data global capability

    # FIXME move kcc and kdc to special registers

    cmove       $c15, $c3                               # save this for the end

    dla         $k0, locals_start
    dli         $k1, locals_size
    csetoffset  $kr1c, $kdc, $k0
    csetboundsexact  $kr1c, $kr1c, $k1                       # kr1c will hold a capability to our locals

#ifdef SMP_ENABLED
	# Send SMP TCs other than 0 to trap to let nano perform init
	get_cpu_id  $t0
#endif

#ifndef HARDWARE_qemu

    #FIXME need to use different PIC for each cpu core

    dli         $t1, PHY_MEM_START_UNCACHED + 0x7f804000
    dli         $t2, STATUS_BIT_TO_BERI_IRQ(INTERRUPTS_SYSTEM_N) //

    // System HW cascades (disabled)
    daddiu      $t3, $t1, (8 * (INTERRUPTS_N_HW - 1))
1:  csd         $t2, $t1, 0($kdc)
    bne         $t1, $t3, 1b
    daddiu      $t1, $t1, 8

    // skip rest of hardware

    daddiu      $t1, $t1, (8 * (64 - INTERRUPTS_N_HW))

    // System SW cascades (disabled)

    daddiu      $t3, $t1, (8 * (INTERRUPTS_N_SW - 1))
1:  csd         $t2, $t1, 0($kdc)
    bne         $t1, $t3, 1b
    daddiu      $t1, $t1, 8

    // Nano SW cascades (enabled)

    dli         $t2, (1 << PIC_CONFIG_OFFSET_E) | (STATUS_BIT_TO_BERI_IRQ(INTERRUPTS_NANO_N))

    daddiu      $t3, $t1, (8 * ((64 - INTERRUPTS_N_SW) - 1))
1:  csd         $t2, $t1, 0($kdc)
    bne         $t1, $t3, 1b
    daddiu      $t1, $t1, 8

#endif

    # Move from BEV

    li          $t2, ~MIPS_CP0_STATUS_BEV
    mfc0        $t1, MIPS_CP0_REG_STATUS
    and         $t1, $t1, $t2
#ifndef HARDWARE_qemu
    // Also enable these interrupts
    ori         $t1, $t1, ((1 << INTERRUPTS_NANO_N) | (1 << INTERRUPTS_SYSTEM_N)) << 8
#endif
    mtc0        $t1, MIPS_CP0_REG_STATUS

#ifdef SMP_ENABLED
    bnez        $t0, nano_smp_trap
    nop
#endif

    csd         $zero, $zero, made_first_res($kr1c)

    dli         $k0, context_table
    dli         $k1, (N_CONTEXTS * CONTEXT_SIZE)
    cincoffset  $kr2c, $kr1c, $k0
    csetboundsexact  $kr2c, $kr2c, $k1                       # Create capability to context_table

    dli         $k0, CONTEXT_SIZE

    cincoffset  $c13, $kr2c, $k0
    csc         $c13, $zero, next_context($kr1c)        # Initialise next context

    csetboundsexact  $c3, $kr2c, $k0
    cincoffset  $c3, $c3, INC_IM_MAX
    csd         $zero, $zero, CONTEXT_OFFSET_STATE($c3)                      # Set state to 0
    csc         $cnull, $zero, CONTEXT_OFFSET_FOUND($c3)    # Set foundation ref to NULL
    csc         $c3, $zero, current_context($kr1c)       # Create first context and make it the current context


    dli         $k1, CONTEXT_TYPE
    csetoffset  $kr2c, $kdc, $k1

    cseal       $c3, $c3, $kr2c                          # Seal the first context

    cnull       $kr2c
    csc         $kr2c, $zero, exception_context($kr1c)   # Set exception context = NULL

    csd         $zero, $zero, exception_level($kr1c)     # Set exception level to 0


    # This setus up stuff for sha256

    dla         $t0, k_words
    dli         $t1, 64 * 4
    csetoffset  $c13, $kdc, $t0
    csetbounds  $c13, $c13, $t1
    csc         $c13, $zero, K_CAP($kr1c)

    # This sets up the physical book


    # t0 will be offset to page entry. t1 len. t2 type. t3 prev.
.macro PHY_NEXT
    dsll        $t2, $t1, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t0, $t0, $t2
.endm
.macro PHY_SET_STATE state
    dli         $t2, \state
    csw         $t2, $t0, 0($kr1c)
.endm
.macro PHY_LINK_LEN
    csd         $t1, $t0, REG_SIZE($kr1c)
.endm
.macro PHY_LINK_LEN_N N
    dli         $t1, \N
    csd         $t1, $t0, REG_SIZE($kr1c)
.endm
.macro PHY_LINK_PREV
    csd         $t3, $t0, (REG_SIZE * 2)($kr1c)
.endm
.macro PHY_NEXT_PREV
    daddu       $t3, $t3, $t1
.endm

    # TODO check a0 is properly aligned?
    #check a0 < RAM_PRE_IO_END - NANO_SIZE.
    dli         $t0, (RAM_PRE_IO_END - NANO_SIZE)
    sltu        $t0, $a0, $t0
    beq         $t0, $zero, nano_kernel_die
    nop

    # First range is nano owned
    dli             $t0, phy_page_table                 # t0 is start of book
    PHY_SET_STATE   page_nano_owned                     # book[0].state = nano owned
    PHY_LINK_LEN_N (NANO_SIZE / PHY_PAGE_SIZE)          # book[0].len = nano pages

    # Then system owned
    dsll            $t2, $t1, PHY_PAGE_ENTRY_SIZE_BITS
    daddu           $t0, $t0, $t2
    PHY_SET_STATE   page_system_owned
    move            $t3, $t1
    dsrl            $t1, $a0, PHY_PAGE_SIZE_BITS        # system pages
    PHY_LINK_LEN

    # Then free low pages (start dirty as boot was around here)
    PHY_NEXT
    PHY_LINK_PREV
    PHY_NEXT_PREV
    dli             $t2, (TOTAL_LOW_RAM_PAGES - (NANO_SIZE / PHY_PAGE_SIZE))
    dsubu           $t1, $t2, $t1   # len = LO_PAGES - NANO_PAGES - SYSTEM_PAGES
    PHY_LINK_LEN
    PHY_SET_STATE   page_dirty

#if RAM_TAGS != 0
    # Then the tag block
    PHY_NEXT
    PHY_LINK_PREV
    PHY_NEXT_PREV
    PHY_SET_STATE   page_nano_owned
    PHY_LINK_LEN_N  (RAM_TAGS / PHY_PAGE_SIZE)
#endif

    # Then IO pages
    PHY_NEXT
    PHY_LINK_PREV
    PHY_NEXT_PREV
    PHY_SET_STATE   page_io
    PHY_LINK_LEN_N  TOTAL_IO_PAGES

#ifdef RAM_SPLIT
    # Then free high pages
    PHY_NEXT
    PHY_LINK_PREV
    PHY_NEXT_PREV
    PHY_LINK_LEN_N  TOTAL_HIGH_RAM_PAGES
#endif

    # Then a terminal page
    PHY_NEXT
    PHY_LINK_PREV
    PHY_SET_STATE   page_nano_owned
    PHY_LINK_LEN_N  1

    # Setup nano IF
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
    cincoffset   $c1, $kr1c, $k0
    csetboundsexact $c1, $c1, $k1
    dli          $k0, (Perm_Load | Perm_Load_Capability)
    candperm     $c1, $c1, $k0


    cseal       $c2, $kr1c, $kr2c                        # Pass a sealed capability to our locals

    # a0 is the start of nano kernel secured memory. a1 is an address we should return to.
    dli         $k0, DEF_DATA_PERMS

    # c0 will be a global data capability to the unsecured memory
    # c17 will be a global code capability to the unsecured memory, with the index passed
    dli         $t0, (PHY_MEM_START_CACHED + NANO_SIZE)          # t0 = start of mem available to system
    daddu       $t2, $a0, $t0                             # t2 = start of free physical mem

    csetoffset  $c13, $kdc, $t0
    csetboundsexact  $c13, $c13, $a0
    candperm    $c13, $c13, $k0
    csetdefault $c13                                    # c13 is the beginning of unmanaged mem

    csetoffset  $c17, $kcc, $t0
    csetboundsexact  $c17, $c17, $a0
    csetoffset  $c17, $c17, $a1
    candperm    $c17, $c17, $k0

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
    # a0 is to allow boot to pass an argument to the kernel. Don't want capabilities or they would have to be checked.
    cclearlo    EncodeReg(all) & ~(EN5(c0, c1, c2, c3, c5))
    cclearhi    EncodeReg(all) & ~(EN6(c17, kdc, kcc, epcc, kr1c, kr2c))

    cjr         $c17
    nop



##########################################################################
# idc/kdc will provide us a capability to our locals in everything below #
# kr1c will hold a capability to our SMP local array.                    #
##########################################################################

.text

#########################################################################################
# register_t modify_hardware_reg(register_t selector, register_t mask, register_t value)
.global modify_hardware_reg
modify_hardware_reg:
#########################################################################################

    sltiu   $t0, $a0, register_mask_table_size
    beqz    $t0, table_end
    dsll    $t0, $a0, 2       # selector * 4 to index words
    clw     $t1, $t0, register_mask_table($idc)
                              # selector * 20 to index trampoline. (20 is 16 + 4)
    dsll    $t3, $a0, 4       # *16
    daddu   $t0, $t0, $t3     # *20  (using the 4 we calculated earlier)


    and     $t1, $t1, $a1     # update mask
    not     $t2, $t1          # ~mask
    and     $t3, $t1, $a2     # mask & value

    daddiu  $t0, $t0, 16      # distance from getpcc to table
    cgetpcc $c1
    cincoffset  $c1, $c1, $t0
    cjr     $c1
    nop

    jump_table:

    #define JUMP_ENTRY(name, num, selector, ...)                        \
    mfc0    $v0, num, selector;                                         \
    and     $t0, $v0, $t2;                                              \
    or      $t0, $t0, $t3;                                              \
    b       table_end;                                                  \
    mtc0    $t0, num, selector;
    NANO_REG_LIST(JUMP_ENTRY)
    #undef JUMP_ENTRY

    table_end:

    cclearlo    EN1(c1)
    CRETURN


####################################### (safe)
# capaiblity get_integer_space_cap(void)
.global get_integer_space_cap
get_integer_space_cap:
#######################################

    candperm    $c3, $kdc, $zero
    CRETURN


####################################### (safe)
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
    CRETURN

################################### (safe)
# page_t* get_book(void)
.global get_book
get_book:
###################################
    dli        $t0, phy_page_table
    dli        $t1, (TOTAL_PHY_PAGES) * PHY_PAGE_ENTRY_SIZE
    cincoffset $c3, $idc, $t0
    dli        $t2, Perm_Load
    csetboundsexact $c3, $c3, $t1
    candperm   $c3, $c3, $t2
    CRETURN

# Notes: Locks the page being split, but not the next page (which will have its back ptr updated) #
# This is O.K. is the prev pointer is never read by the nano kernel. Only updated. #
################################################################# (safe)
# void split_phy_page_range(register_t pagen, register_t new_len)
.global split_phy_page_range
split_phy_page_range:
#################################################################
    beqz        $a1, split_phy_er
    dli         $t0, TOTAL_PHY_PAGES                # check index in range
    sltu        $t0, $a0, $t0
    beq         $t0, $zero, split_phy_er                # $a0 >= $t0
    dli         $t0, phy_page_table                     # $t0 = start of book
    dsll        $t1, $a0, PHY_PAGE_ENTRY_SIZE_BITS      # $t1 = offset for pagen
    daddu       $t0, $t0, $t1                           # $t0 = book + pagen

    cincoffset  $c3, $idc, $t0                          # c3 is book entry

    dli         $t8, page_transaction                   # to set to
1:  cllw        $t9, $c3                                # lock physcial page
    daddiu      $t9, $t9, -(page_transaction-1)         # make sure les than transaction
    bgtz        $t9, split_phy_er
    daddiu      $t9, $t9, (page_transaction-1)
    cscw        $t3, $t8, $c3                           # set to transaction. t9 holds old state
    beqz        $t3, 1b
    nop

    cld         $t2, $zero, (PHY_PAGE_OFFSET_len)($c3)  # $t2 = book[pagen].len
    sltu        $t3, $a1, $t2
    beq         $t3, $zero, split_phy_undo              # $a1 >= $t2
    dsll        $t3, $t2, PHY_PAGE_ENTRY_SIZE_BITS      # t3 is byte offset from entry to next
    cincoffset  $c4, $c3, $t3                           # c4 is last book entry
    dsubu       $t3, $t2, $a1                           # $t3 is length of next block

    csd         $a1, $zero, (REG_SIZE)($c3)             # book[pagen].len = new_len

    dsll        $t1, $a1, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset  $c5, $c3, $t1                           # c5 is new entry

    csw         $t8, $zero, (PHY_PAGE_OFFSET_status)($c5)   # book[new].state = transaction
    csd         $t3, $zero, (PHY_PAGE_OFFSET_len)($c5)      # book[new].len = book[pagen].len - new_len
    csd         $a0, $zero, (PHY_PAGE_OFFSET_prev)($c5)     # book[new].prev = pagen

    daddu       $t1, $a0, $a1
    csd         $t1, $zero, (2*REG_SIZE)($c4)           # book[next].prev = pagen + new_len

    csw         $t9, $zero, 0($c5)                      # book[new].state = book[pagen].state

split_phy_undo:
    csw         $t9, $zero, 0($c3)                      # unlock page
split_phy_er:
    cclearlo    EN3(c3,c4,c5)
    CRETURN

# Notes: Locks pagen and the next page. Not the page after that even though the back pointer is updated #
############################################## (safe)
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
    daddiu      $t8, $t8, -(page_transaction-1)         # make sure les than transaction
    bgtz        $t8, merge_phy_page_range_end
    daddiu      $t8, $t8, (page_transaction-1)
    cscw        $t3, $a2, $c3
    beqz        $t3, 1b
    nop

    cld         $t2, $t0, (PHY_PAGE_OFFSET_len)($idc)              # $t2 = book[pagen].len
    beqz        $t2, merge_phy_er_1
    sll         $t1, $t2, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t1, $t1, $t0                           # t1 = book[pagen + len]

    cincoffset  $c4, $idc, $t1                          # c3 is pointer to first page
1:  cllw        $t9, $c4                                # t9 is state of second page
    bne         $t8, $t9, merge_phy_er_1
    cscw        $t3, $a2, $c4
    beqz        $t3, 1b
    nop

    cld         $t3, $t1, PHY_PAGE_OFFSET_len($idc)    # t3 = len2

    beqz        $t3, merge_phy_er_2

    daddu       $t2, $t2, $t3
    sll         $t3, $t3, PHY_PAGE_ENTRY_SIZE_BITS
    daddu       $t3, $t3, $t1                           # t3 = book[pagen + len + len2]

    csd         $zero, $t1, PHY_PAGE_OFFSET_len($idc)   # book[page + len].len = 0
    csd         $t2, $t0, PHY_PAGE_OFFSET_len($idc)     # book[page].len += len2
    csd         $a0, $t3, (PHY_PAGE_OFFSET_prev)($idc)  # book[page + len + len2].prev = pagen

    b           merge_phy_er_1                          # finish by unlocking first page
    csw         $zero, $zero, 0($c4)                    # unlock second page

merge_phy_er_2:
    csw         $t9, $zero, PHY_PAGE_OFFSET_status($c4)
merge_phy_er_1:
    csw         $t8, $zero, PHY_PAGE_OFFSET_status($c3)
merge_phy_page_range_end:
    cclearlo    EN2(c3,c4)
    CRETURN

################################### (safe)
# void zero_page_range(register_t pagen)
.global zero_page_range
zero_page_range:
###################################

    dli         $t0, TOTAL_PHY_PAGES                # check in range
    sltu        $t0, $a0, $t0
    beqz        $t0, zero_er
    dli         $t0, phy_page_table
    cincoffset  $c3, $idc, $t0
    dsll        $t3, $a0, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset  $c3, $c3, $t3                       # c3 is book+page_n

    # must go dirty -> transaction -> cleaning -> unused
    dli         $a1, page_dirty
    dli         $a2, page_transaction

1:  cllw        $t3, $c3                            # t3 = state
    bne         $t3, $a1, zero_er                   # check dirty
    nop
    cscw        $t2, $a2, $c3
    beqz        $t2, 1b
    cld         $t2, $zero, PHY_PAGE_OFFSET_len($c3)    # t2 = len

    beqz        $t2, zero_er_1                      # abort transaction
    dli         $a2, page_cleaning
    csw         $a2, $zero, 0($c3)                  # set state cleaning
#  From here on we are fine to zero. $a0 is page_n. $t2 is len

    sync
    cld         $t0, $zero, need_shootdown($idc)                            #ra/t0 clobbered
    bnez        $t0, tlb_shootdown_local
    li          $t3, 1
    b           no_shootdown

# Send a signal to every core to invalidate their TLB. Can also be called fr

###########################
# void tlb_shootdown(void)
.global tlb_shootdown
tlb_shootdown:
###########################
    li          $t3, 0
tlb_shootdown_local:
#ifdef SMP_ENABLED

    # Set all flags to 0
    dli         $t0, (context_array_size) * (SMP_CORES-1)
1:  csd         $zero, $t0, since_shotdown($idc)
    bnez        $t0, 1b
    daddiu      $t0, $t0, -(context_array_size)

# Send shootdown IPC
#ifndef HARDWARE_qemu
    dli         $t1, (PHY_MEM_START_UNCACHED + PIC_CONFIG_START + (8 * 1024) + 128 + INTERRUPTS_NANO_PIC_SHOOTDOWN_OFF)
    dli         $t8, INTERRUPTS_NANO_PIC_SHOOTDOWN_BIT
#endif

    dli         $t0, SMP_CORES-1
1:
#ifdef HARDWARE_qemu
    SET_VPE_CONTROL_TargTC $t1, $t0
    SEND_IPI(t1, 0)
#else
    csb         $t8, $t1, 0($kdc)
    daddiu      $t1, $t1, PIC_CONFIG_SZ
#endif
    bnez        $t0, 1b
    daddiu      $t0, -1

    # Check all flags 1
    dli         $t0, (context_array_size) * (SMP_CORES-1)

2:  YIELD
    sync
1:  cld         $t1, $t0, since_shotdown($idc)
    beqz        $t1, 2b
    nop
    bnez        $t0, 1b
    daddiu      $t0, $t0, -(context_array_size)
#else
    invalidate_tlb $t0, $t1
#endif
    bnez        $t3, no_shootdown
    csd         $zero, $zero, need_shootdown($idc)
    CRETURN

no_shootdown:
    dli         $t1, PHY_MEM_START_CACHED
    dsll        $a0, $a0, PHY_PAGE_SIZE_BITS
    daddu       $a0, $a0, $t1                          # a0 is the address of page

    move        $t3, $ra                                #ra/t0 clobbered
1:                                                      # call zero len times
    jal         nano_zero_page
    move        $t0, $a0
    daddiu      $t2, $t2, -1
    bnez        $t2, 1b
    daddiu      $a0, $a0, PHY_PAGE_SIZE

    move        $ra, $t3                             # restore ra
    move        $a1, $zero

    zero_er_1:

    csw         $a1, $zero, 0($c3)                # set state clean or dirty depending how we got here

    zero_er:
    cclearlo    EN2(c3, c13)
    CRETURN

################################### (safe)
# ptable get_top_level_table(void)
.global get_top_level_table
get_top_level_table:
###################################
    dli         $t0, top_virt_page
    dli         $t1, PAGE_TABLE_SIZE
    cincoffset  $c3, $idc, $t0
    dli         $t0, VTABLE_TYPE_L0
    csetboundsexact  $c3, $c3, $t1
    csetoffset  $c4, $kdc, $t0
    cseal       $c3, $c3, $c4

    clearlo     EN1(c4)
    CRETURN

##################################### (safe)
# res_t make_first_reservation(void)
.global make_first_reservation
make_first_reservation:
#####################################
    # TODO we should actually revoke the initial reservation in case the boot loader made a mistake

    cincoffset  $c4, $idc, made_first_res
1:  clld        $t2, $c4
    bnez        $t2, make_first_er
    dli         $t2, 1
    cscd        $t2, $t2, $c4
    beqz        $t2, 1b

    dli         $t2, VIRT_MEM_START
    dli         $t1, VIRT_MEM_SIZE
    csetoffset  $c3, $kdc, $t2
    dli         $t0, DEF_DATA_PERMS
    candperm    $c3, $c3, $t0                     # c3 is a cap to all of virtual mem

    dli         $t0, RES_META_SIZE                # amount of space we lost to meta data
    dsubu       $t2, $t1, $t0                     # remaining length
    dli         $t1, RES_TYPE                     # sealing type

    csetoffset  $c4, $kdc, $t1
    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3)     # set state to open
    csd         $t2, $zero, (RES_LENGTH_OFFSET)($c3)        # set length
    csd         $zero, $zero, (RES_PID_OFFSET)($c3)         # set parent to none

    csetboundsexact  $c3, $c3, $t0
    cseal       $c3, $c3, $c4                     # handle

make_first_er:
    cclearlo    EN2(c4, c5)
    CRETURN

##################################################################### (TODO unsafe? only when exl = 0)
# void context_switch(context_t restore_from);
.global context_switch
context_switch:
#####################################################################

    # Enter an exception level to turn off interrupts. We must at the least switch from restore_from.
    # However we might switch to the exception_context
    li      $a4, 0                                      # a4 can't get clobbered by an exception
context_switch_local_entry: # void context_switch_local_entry(a4: reg_t cause)

    SET_EXL $t0

    # TODO unseal these (if sealed)
    # WARN: We have changed idc here. If we ever get to user space this will leak. Must set in_switch before any exceptions
    cmove   $epcc, $c17
    cmove   $idc, $c18
    move    $k0, $a4

context_switch_exception_entry: # void context_switch_exception_entry(k0: reg_t cause)
    cgetcause   $k1
context_switch_exception_entry_custom_ccause: # (k0: reg_t cause, k1: reg_t ccause

    csd     $k1, $zero, last_exception_ccause($kr1c)
    csd     $k0, $zero, last_exception_cause($kr1c)
    dmfc0   $k1, cp0_badvaddr                           # Hoisted setting this here from switch_exception
                                                        # The value can change if we take a nested vmem fault
    csd     $k1, $zero, last_bad_vaddr($kr1c)

    li      $k1, 1
    csd     $k1, $zero, in_switch($kr1c)

    csc     $epcc, $zero, switch_cap_tmp($kr1c)
    clc     $kr2c, $zero, current_context($kr1c)        # kr2c not clobbered by refill

    cgetpcc $epcc                                       # Restart point
    SYNC_EPCC
    QEMU_BUG_FIX $k1
    cld     $k1, $zero, (-INC_IM_MAX)($kr2c)            # Touch frame to cause TLB fault
    cld     $k1, $zero, (CONTEXT_OFFSET_STATE)($kr2c)
                                                        # End restart point. Can't have TLB error here.

    cld     $k0, $zero, last_exception_cause($kr1c)           # must be reloaded after restart

    csc     $c1, $zero, (FRAME_C1_OFFSET-INC_IM_MAX)($kr2c)
    clc     $c1, $zero, switch_cap_tmp($kr1c)           # our first clobber. No exceptions now till next checkpoint.
    csc     $idc, $zero, (FRAME_idc_OFFSET-INC_IM_MAX)($kr2c)
    csc     $c1, $zero, (FRAME_pcc_OFFSET-INC_IM_MAX)($kr2c)

    save_reg_frame_no_idc $kr2c, $k1                    # Save state (pcc and idc already saved and c1 already saved)

    bnez     $k0, switch_exception                      # set to exception cause if we used the local entry
    cincoffset  $c13 , $kdc, CONTEXT_TYPE

    b        switch_restore_final
    cunseal  $c3, $c3, $c13                             # meant to be in delay

switch_exception:                                       # void switch_exception(context_t victim, reg_t cause)


                # If we were doing it properly we would modify it to have the faulting instruction be from the new
                # activation. However, for now as only the bits to check the type of
                # interrupt we will just use this.

    clc     $c5, $zero, current_context($kr1c)          # Current context is the victim
    cseal   $c4, $c5, $c13                              # Seal c3 to pass to exception handler
    clc     $c3, $zero, exception_context($kr1c)        # load exception context
    CEXEQ   $t0, $c3, $c5
    bnez    $t0, nano_kernel_die
    csc     $c4, $zero, victim_context($kr1c)           # store victim

    # HERE # If you want to restore the exception context with magic values, store via c3

    # Restore everything, we dont have a register spare for $c0 so set default while restoring
    # We use exception registers here. These are not used by the critical section check in exception.S

    .macro crestore_setc0 greg, offset, frame
        crestore \greg, \offset, \frame
        .if \offset == 0
            csetdefault \greg
        .endif
    .endm

switch_restore_final:                                   # void switch_restore(context_t restore_from)
    csd     $zero, $zero, exception_level($kr1c)        # critical_state.level = 0 (should be in delay)
    csd     $zero, $zero, supressed($kr1c)
    csc     $c3, $zero, current_context($kr1c)          # set c3 it as the current context
    csd     $k0, $zero, last_exception_cause($kr1c)     # k0 could get clobbered by TLB exception

    # Current context has been set. We must restart from here
    li      $t0, 1
    csd     $t0, $zero, in_switch($kr1c)

    # $c3 will be overwritten so use $kr2c.
    cmove $kr2c, $c3

    cgetpcc $epcc                                       # Restart point
    SYNC_EPCC
    QEMU_BUG_FIX $a0

    # Force exception early
    cld         $a0, $zero, CONTEXT_OFFSET_STATE($kr2c)
    bnez        $a0, nano_kernel_die                    # Check this context is still alive
    cld         $k1, $zero, (-INC_IM_MAX)($kr2c)
    # No exceptions from now on

    restore_reg_frame $kr2c, $k1, $epcc

    # From here we can only do a restore, and we are about to overwrite all registers.
    # It should be safe to mark this core as having switched for revoke signal
    li          $k1, 1
    csd         $k1, $zero, since_switched($kr1c)

    # We can have no more exceptions now so its safe to deset the flag
    csd         $zero, $zero, in_switch($kr1c)
    cld         $k0, $zero, last_exception_cause($kr1c)       # restore k0 now we are not touching virtual memory

return:

    # This tackles a qemu bug
    QEMU_BUG_FIX $k1
    SYNC_EPCC
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

################################################# (safe)
# void get_last_exception(exection_cause_t* out)
.global get_last_exception
get_last_exception:
#################################################
    cld        $v0, $zero, last_exception_cause($kr1c)
    beqz       $v0, 1f
    clc        $c4, $zero, victim_context($kr1c)
    cld        $t1, $zero, last_exception_ccause($kr1c)
    cld        $t2, $zero, last_bad_vaddr($kr1c)
    cld        $t3, $zero, exception_level($kr1c)
    csc        $c4, $zero, 0($c3)
    csd        $v0, $zero, CAP_SIZE($c3)
    csd        $t1, $zero, (CAP_SIZE+REG_SIZE)($c3)
    csd        $t2, $zero, (CAP_SIZE+(2*REG_SIZE))($c3)
    csd        $t3, $zero, (CAP_SIZE+(3*REG_SIZE))($c3)
1:
    cclearlo    EN2(c3,c4) # If there was no exception do not leak any other field
    CRETURN


################################################################################################################ (safe)
# capability get_phy_page(register_t page_n, register_t cached, register_t npages, cap_pair* out, register_t IO)
.global get_phy_page
get_phy_page:
################################################################################################################

    beqz        $a2, get_phy_page_end
    dli         $t0, TOTAL_PHY_PAGES
    cmove       $c5, $c3
    cnull       $c3
    cnull       $c4

    sltu        $t0, $a0, $t0
    beq         $t0, $zero, get_phy_page_end            # $a0 >= $t0
    dsll         $t0, $a0, PHY_PAGE_ENTRY_SIZE_BITS
    dli         $t1, phy_page_table
    daddu       $t0, $t0, $t1                           # $t0 is the offset of our phy entry
    cincoffset  $c6, $idc, $t0                          # $c6 our entry


    dli         $t1, page_system_owned
    dli         $t2, page_io
    movn        $t1, $t2, $a3                           # t1 is the state we set to on success

# Set to transacting

1:  cllw        $t3, $c6                                # t3 is what we will restore on error
    beqz        $t3, 2f                                 # must be free
    li          $t2, page_transaction
    bne         $t1, $t3, get_phy_page_end              # or in the state we will set it to
2:  nop
    cscw        $t0, $t2, $c6
    beqz        $t0, 1b
    cld         $t2, $zero, PHY_PAGE_OFFSET_len($c6)    # $t2 = len of record

    bne         $t2, $a2, get_phy_page_backout          # $t2 must be npages

    move        $t3, $t1                                # set to system owned / io

    dli         $t1, PHY_MEM_START_CACHED_TOP
    dli         $t2, PHY_MEM_START_UNCACHED_TOP
    movz        $t1, $t2, $a1                           # 0: uncached. ow: cached
    dsll        $t1, $t1, TOP_ADDR_SHIFT

    dsll        $t2, $a0, PHY_PAGE_SIZE_BITS
    dsll        $t0, $a2, PHY_PAGE_SIZE_BITS
    daddu       $t1, $t1, $t2                           # t1 is the offset required for our cap
    csetoffset  $c3, $kdc, $t1
    csetoffset  $c4, $kcc, $t1
    csetboundsexact  $c3, $c3, $t0                           # bounds number of pages requested
    csetboundsexact  $c4, $c4, $t0

    dli         $t0, DEF_DATA_PERMS   #t0 is the permission mask
    dli         $t1, (Perm_Load | Perm_Store)
    movn        $t0, $t1, $a3

    candperm    $c3, $c3, $t0
    candperm    $c4, $c4, $t0

    csc         $c3, $zero, CAP_SIZE($c5)
    csc         $c4, $zero, 0($c5)

get_phy_page_backout:
    csw         $t3, $zero, PHY_PAGE_OFFSET_status($c6)
get_phy_page_end:
    cclearlo    EN1 (c6)
    CRETURN


########################################################
# context_t create_context(reg_frame_t* initial_state, res_t res);
.global create_context
create_context:
########################################################
# TODO allow user to provide reservation
    cbtu        $c4, create_allocate_static
    cmove       $c13, $c3

create_allocate_dynamic:
    cmove       $c3, $c4
    SUBROUTINE_TAKE # Subroutine will clober c3,c4,c5,c14,c15,t*,a7. c3_in=res.c3_out=code.c4_out=data
    b           allocated

create_allocate_static:
    dli         $t0,  CONTEXT_SIZE                          # needed in delay slot as well
1:  cincoffset  $c6, $idc, next_context
    cllc        $c4, $c6
    cincoffset  $c5,  $c4, $t0
    cscc        $t3, $c5,  $c6                              # increment next context
    beqz        $t3, 1b

allocated:
    csetboundsexact  $c4, $c4, $t0                               # will fail if we were out of space
    cincoffset  $c4, $c4, (INC_IM_MAX)
    csd         $zero, $zero, CONTEXT_OFFSET_STATE($c4)          # set state to allocated for new context
    csc         $cnull, $zero, CONTEXT_OFFSET_FOUND($c4)         # set foundation ref to null
    # memcpy a0 bytes from c3 to c4
    dli         $t0, 0
    dli         $a0, CHERI_FRAME_SIZE
memcpy_loop:
    clc         $c5, $t0, 0($c13)
    daddiu      $t0, $t0, CAP_SIZE
    bne         $t0, $a0, memcpy_loop
    csc         $c5, $t0, (-(CAP_SIZE + INC_IM_MAX))($c4)

    cincoffset  $c3, $kdc, CONTEXT_TYPE                    # Load sealing capability
    cseal       $c3, $c4, $c3                              # Return sealed cap

1:
    cclearlo    EN6(c4,c5,c6,c13,c14,c15)                              # c3 is a return value
    CRETURN



########################################################################
# context_t destroy_context(context_t context, context_t restore_from);
.global destroy_context
destroy_context:
########################################################################

    cincoffset  $c5, $kdc, CONTEXT_TYPE                     # Load sealing capability
    cunseal     $c3, $c3, $c5                               # unseal context we are destroying
    clc         $c6, $zero, current_context($kr1c)          # load current context
    ceq         $t1, $c3, $c6                               # t1 = 1 if we are deleting ourselves
    dli         $t2, CONTEXT_STATE_DESTROYED
    dli         $t0, CONTEXT_OFFSET_STATE
    cincoffset  $c6, $c3, $t0

1:  clld        $t3, $c6
    bnez        $t3, destroy_context_end                    # must be created
    cscd        $t3, $t2, $c6
    beqz        $t3, 1b
    nop

    beqz        $t1, destroy_context_end                    # if we are not destroying ourselves we can end now

    # If we are here we are destroying ourselves, thus we should restore restore_from
    cunseal     $c3, $c4, $c5
    SET_EXL     $t1

    b           switch_restore_final
    li          $k0, 0

destroy_context_end:
    cclearlo    EN4(c3,c4,c5,c6)
    CRETURN

################################# (safe)
# uint8_t critical_section_enter();
.global critical_section_enter
critical_section_enter:
#################################
    cld        $v0, $zero, exception_level($kr1c)
    daddiu     $v0, 1
    csd        $v0, $zero, exception_level($kr1c) # This has efftively turned off interrupts so getting cpu_id is ok
    dmfc0      $v0, $15, 1
    andi       $v0, $v0, 0xFF
    CRETURN



# TODO this, like a few other functions, set EXL. This might be unsafe if we take an interrupt due to
# TODO settin exl not being atomic.

################################# (safe)
# void critical_section_exit();
.global critical_section_exit
critical_section_exit:
#################################

    cld        $v0, $zero, exception_level($kr1c)
    daddiu     $v0, -1
    bnez       $v0, kernel_critical_section_exit_end
    csd        $v0, $zero, exception_level($kr1c)            # decrement level could take exception now

    # Re-enable interrupts if we turned them off.

    cld        $t2, $zero, supressed($kr1c)
    beqz       $t2, kernel_critical_section_exit_end
    csd        $zero, $zero, supressed($kr1c)

    mfc0       $t0, MIPS_CP0_REG_STATUS
    ori        $t0, $t0, 1
    mtc0       $t0, MIPS_CP0_REG_STATUS                     # exception will happen here

kernel_critical_section_exit_end:
    CRETURN




################################################################### (safe)
# void set_exception_handler(context_t context, register_t cpu_id);
.global set_exception_handler
set_exception_handler:
###################################################################
    sltiu       $t1, $a0, SMP_CORES
    beqz        $t1, 1f

    cincoffset  $c4, $kdc, CONTEXT_TYPE
    cunseal     $c3, $c3, $c4

#ifdef SMP_ENABLED
    dli         $t0, context_array_size
    multu       $t0, $a0
    mflo        $t0
    daddiu      $t0, current_context

    csc         $c3, $t0, exception_context($idc)
    sync
#else
    csc         $c3, $zero, exception_context($idc)
#endif
1:
    cclearlo    EN2(c3,c4)
    CRETURN


############################# (safe)
# void nano_kernel_die(void)
.global nano_kernel_die
nano_kernel_die:
#############################
    li  $zero, 0xbeef
    li  $v0,   0xbad
    mfc0        $a0, MIPS_CP0_REG_CAUSE
    cgetcause   $a1
    cgetepcc    $c3

    dli $k0,  ((0x1f000000 + 0x00500) | 0x9000000000000000)
    li  $k1,  0x42
1:
    b   1b
    csb $k1, $k0, 0($kdc)

############################################# (TODO (maybe remove?)
# capability get_userdata_for_res(res_t res)
.global get_userdata_for_res
get_userdata_for_res:
#############################################
    cgetoffset  $t0, $c3
    bnez        $t0, take_er
    dli         $t1, RES_PRIV_SIZE
    cincoffset  $c4, $kdc, RES_TYPE
    dli         $t2, RES_USER_SIZE
    cunseal     $c3, $c3, $c4
    cincoffset  $c3, $c3, $t1
    csetboundsexact  $c3, $c3, $t2
    cclearlo    EN1(c4)
    CRETURN



# WARN: This is called as subroutine. Be careful on register usage. Don't clobber argument registers #
############################################ (safe)
# void rescap_take(res_t res, cap_pair* out)
.global rescap_take
rescap_take:
############################################
    cmove       $c5, $c4
    cincoffset  $c4, $kdc, RES_TYPE         # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res
    cgetoffset  $t3, $c3                    # t3 is non zero if using subfields
    csetoffset  $c3, $c3, $zero

    cgetbase    $t0, $c3
    daddiu      $t0, $t0, RES_META_SIZE     # get base
    li          $t8, RES_SIZE_NOT_FIELD

1:  LOADL_RES_STATE $t2, $c3                # get state
    bnez        $t3, take_sub
    cld         $t1, $zero, RES_LENGTH_OFFSET($c3)      # get length (protected by load linked)
    bnez        $t2, take_er                # error if not open
    dli         $t2, res_taken
    STOREC_RES_STATE $t3, $t2, $c3          # try set to taken
    beqz        $t3, 1b
    csb         $t8, $zero, RES_SUBFIELD_SIZE_OFFSET($c3) # set not subfield by setting size to not field
                                                          # this will not matter even if this store cond fails
                                                          # FIXME: This was meant to be before state change, does not work on fpga
    b           take_make
    nop

take_sub:
# t0 is base of ALL of the reservation. t1 is length of ALL. t2 is state. t3 is index+1.
# We don't really care about a merge race. A revoke race should be impossible as c3 will be detagged.
# We must handle the take race however. Do this will conditionals on the resfield bits

    dli         $t8, res_taken
    bne         $t2, $t8, take_er           # subfields are marked taken. We should look at the bitmap.
    daddiu      $t3, $t3, -1

    # first calculate byte and mask
    dli         $t2, 1
    andi        $t8, $t3, 0b111
    dsllv       $t8, $t2, $t8               # t8 is 1 byte mask for bitfield
    dsrl        $t2, $t3, 3                 # t2 is byte

    # set bit or error if already set
    cincoffset  $c3, $c3, RES_SUBFIELD_BITMAP_OFFSET
    cincoffset  $c3, $c3, $t2

2:  cllb        $t9, $c3
    and         $a7, $t8, $t9
    bnez        $a7, take_er
    or          $t9, $t9, $t8
    cscb        $t9, $t9, $c3
    beqz        $t9, 2b
    nop

    # calculate base offset
    neg         $t2, $t2
    clb         $t2, $t2, (RES_SUBFIELD_SIZE_OFFSET-RES_SUBFIELD_BITMAP_OFFSET)($c3)   # t2 is the scale
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

    csetboundsexact  $c3, $c3, $t1
    csetboundsexact  $c4, $c4, $t1

    candperm    $c3, $c3, $t2
    candperm    $c4, $c4, $t2

    cbtu        $c5, take_end
    nop

    csc         $c3, $zero, 0($c5)
    csc         $c4, $zero, CAP_SIZE($c5)

take_end:
    CRETURN
take_er:
    clearlo     EN2(c3, c4)
    CRETURN

################################################## (safe)
# res_t rescap_getsub(res_t res, register_t index)
.global rescap_getsub
rescap_getsub:
##################################################

    cgetoffset  $t0, $c3
    bnez        $t0, getsub_er          # can only sub the main reservation
    cincoffset  $c4, $kdc, RES_TYPE
    cunseal     $c5, $c3, $c4

    clb         $t0, $zero, RES_SUBFIELD_SIZE_OFFSET($c5) # size field indicates whether or not it is a subfield
    daddiu      $t0, $t0, -(RES_SIZE_NOT_FIELD)
    beqz        $t0, getsub_er

    sltiu       $t0, $a0, RES_SUBFIELD_BITMAP_BITS
    beqz        $t0, getsub_er          # can only have an index that will fit in the bitfield

    daddiu      $a0, $a0, 1             # 0 means it is the parent of the subfield
    csetoffset  $c3, $c5, $a0           # offset field encodes sub
    cseal       $c3, $c3, $c4

getsub_er:
    cclearlo    EN2(c4, c5)
    CRETURN


#################################################### (safe)
# res_t rescap_splitsub(res_t res, register_t scale)
.global rescap_splitsub
rescap_splitsub:
####################################################

    cincoffset  $c4, $kdc, RES_TYPE              # create unsealing cap
    cunseal     $c3, $c3, $c4                    # unseal res
1:  LOADL_RES_STATE $t0, $c3                     # get state
    bnez        $t0, take_er                                    # error if not open
    dli         $t0, res_trans
    STOREC_RES_STATE $t0, $t0, $c3               # set to transacting to lock this in, but not make useable
    beqz        $t0, 1b

    csh         $zero, $zero, RES_SUBFIELD_BITMAP_OFFSET($c3)
    csw         $zero, $zero, (RES_SUBFIELD_BITMAP_OFFSET+2)($c3)
    csd         $zero, $zero, (RES_SUBFIELD_BITMAP_OFFSET+6)($c3)      # set bitfield to all not taken
    csb         $a0, $zero, (RES_SUBFIELD_SIZE_OFFSET)($c3)             # set scale

    sync
    dli         $t0, res_taken
    STORE_RES_STATE $t0, $zero, RES_STATE_OFFSET($c3)
    cseal       $c3, $c3, $c4

    cclearlo    EN1(c4)
    CRETURN

#################################### (safe)
# size_t rescap_nfo(res_t res)
.global rescap_nfo
rescap_nfo:
####################################
    li          $v0, 0
    cincoffset  $c4, $kdc, RES_TYPE         # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res

    cgetoffset  $t3, $c3                    # get sub-id if exists
    csetoffset  $c3, $c3, $zero
    cld         $v0, $zero, RES_LENGTH_OFFSET($c3)       # get length

    cgetbase    $v1, $c3
    beqz        $t3, normal_info
    daddiu      $v1, RES_META_SIZE

    dli         $t2, 1
    clb         $t0, $zero, RES_SUBFIELD_SIZE_OFFSET($c3)
    dsllv       $v0, $t2, $t0               # set new bounds
    daddiu      $t3, $t3, -1
    dsllv       $t3, $t3, $t0
    daddu       $v1, $v1, $t3

    normal_info:
    cclearlo    EN2(c3,c4)
    CRETURN


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


###################################### (safe)
# void rescap_revoke_start(res_t res);
.global rescap_revoke_start
rescap_revoke_start:
######################################
    cincoffset  $c4, $kdc, RES_TYPE         # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res

    cgetoffset  $t0, $c3                    # cant revoke sub-reservations
    bnez        $t0, revoke_start_er
    li          $t2, res_taken

1:  LOADL_RES_STATE $t0, $c3
    bne         $t0, $t2, revoke_start_er  # check this reservation is taken
    li          $t0, res_trans
    STOREC_RES_STATE $t0, $t0, $c3         # set to transaction so nothing else can modify
    beqz        $t0, 1b

    cgetbase    $a2, $c3                    #a2 is the base of our revoke
    cld         $t1, $zero, RES_LENGTH_OFFSET($c3)
    daddiu      $a4, $t1, RES_META_SIZE     #a4 is the size of revoke
    daddu        $a3, $a2, $a4               #a3 is the end of our revoke (not inclusive)

    andi        $t0, $a2, (PHY_PAGE_SIZE-1)
    bnez        $t0, revoke_start_er_backout1              # base of revocation must be page aligned
    andi        $t0, $a4, (PHY_PAGE_SIZE-1)
    bnez        $t0, revoke_start_er_backout1              # size of revocation must be page aligned

    cincoffset  $c5, $idc, revoke_state
2:  clld        $t0, $c5
    bnez        $t0, revoke_start_er_backout1              # check we are not already revoking
    dli         $t0, REVOKE_STATE_TRANS
    cscd        $t0, $t0, $c5
    beqz        $t0, 2b

    # We are locked in at this point

    li          $t2, res_revoking                           # when we finish set from transacting to revoking

    csd         $a2, $zero, revoke_base($idc)
    csd         $a3, $zero, revoke_bound($idc)
    sync                                                    # make sure we have set base and bound before advertising started

    li          $t1, REVOKE_STATE_STARTED
    csd         $t1, $zero, 0($c5) # Set revoke state to 1

revoke_start_er_backout1:
    STORE_RES_STATE $t2, $zero, RES_STATE_OFFSET($c3) # Mark reservation as revoking or open again
revoke_start_er:
    cclearlo    EN4(c3, c4, c5, c6)
    CRETURN


#define REVOKE_ER_NOT_STARTED       1
#define REVOKE_ER_HAS_ENTRY         2
#define REVOKE_ER_L0_SKIP_START     3
#define REVOKE_ER_L0_SKIP_END       4
#define REVOKE_ER_L1_SKIP_START     5
#define REVOKE_ER_L1_SKIP_END       6
#define REVOKE_ER_L0_SKIP_START2     7
#define REVOKE_ER_L0_SKIP_END2       8
#define REVOKE_ER_L1_SKIP_START2     9
#define REVOKE_ER_L1_SKIP_END2       10

#define SET_ER(X) li $s0, X

####################################### (TODO unsafe)
# void rescap_revoke_finish(res_t res);
.global rescap_revoke_finish
rescap_revoke_finish:
#######################################

    cincoffset  $c4, $idc, revoke_state
    inttoc      $c9, $s0
    li          $s0, REVOKE_ER_NOT_STARTED

    clld        $t0, $c4
    daddiu      $t0, $t0, (-(REVOKE_STATE_STARTED))         # revoke must have been started
    bnez        $t0, revoke_er
    li          $t0, REVOKE_STATE_REVOKING
    cscd        $t0, $t0, $c4                               # set revoke state to revoking

    cld         $a2, $zero, revoke_base($idc)
    cld         $a3, $zero, revoke_bound($idc)

    # Blocking writing of the capability - should do as early as possible #
#ifndef CHERI_LEGACY_COMPAT
    dli         $t0, DEF_DATA_PERMS
    dmtc0       $a2, MIPS_CP0_REG_REVOKE, MIPS_CP0_REG_REVOKE_BASE
    dmtc0       $a3, MIPS_CP0_REG_REVOKE, MIPS_CP0_REG_REVOKE_BOUND
    dmtc0       $t0, MIPS_CP0_REG_REVOKE, MIPS_CP0_REG_REVOKE_PERMS # only revoke Writable and Executable caps, not sealing caps
#endif

#ifdef SMP_ENABLED
    # Now blocking has started we can set that we need suspend/restore
    dli         $t0, (context_array_size) * (SMP_CORES-1)
1:  csd         $zero, $t0, since_switched($idc)
    bnez        $t0, 1b
    daddiu      $t0, $t0, -(context_array_size)
#endif

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
    # t3 and t8 are exclusive when calculated, t9 is inclusive
    EXTRACT_AND_SCALE_L0 $a3, $t3, PAGE_TABLE_ENT_BITS
    daddiu     $t3, $t3, PAGE_TABLE_ENT_SIZE
    # a1 has the used flag
    dli         $a1, VTABLE_ENTRY_USED
    # a4 is the transacting flag. Normally we would abort if we find it. Revoke is allowed to take as long as it likes
    # So instead we will loop until the page is not transacting
    dli         $a4, VTABLE_ENTRY_TRAN

    # The following walks through the vtables and checks all entries covered by the reservation are used,
    # or if they are un-used sets them to used. If an L2 mapping is present, we get an error.

    # c8 is the last thing we had to load link

# FIXME. Need to set pages we are loop through to transacting to stop races

b   l0_loop_start
    nop

    l0_retry_yield:
        YIELD
        b l0_retry
        nop
    l1_retry_yield:
        YIELD
        b l1_retry
        nop
    l2_retry_yield:
        YIELD
        b l2_retry
        nop


    l0_loop_start:
        cincoffset  $c8, $c5, $t0
l0_retry: clld        $a0, $c8
        beq         $a0, $a4, l0_retry_yield
        nop
        beq         $a0, $a1, l0_loop_footer    # if used everything is ok
        daddiu      $t0, $t0, PAGE_TABLE_ENT_SIZE
        beqz        $a0, 1f                     # if un-used must set to used

        cfromptr    $c6, $kdc, $a0              # otherwise go into inner loop
        bne         $t0, $t3, l1_loop_start
        dli         $t8, (PAGE_TABLE_ENT_PER_TABLE << PAGE_TABLE_ENT_BITS)
        EXTRACT_AND_SCALE_L1 $a3, $t8, PAGE_TABLE_ENT_BITS
        daddiu      $t8, $t8, PAGE_TABLE_ENT_SIZE

        l1_loop_start:
            cincoffset  $c8, $c6, $t1
l1_retry:   clld        $a0, $c8
            beq         $a0, $a4, l1_retry_yield
            nop
            beq         $a0, $a1, l1_loop_footer    # if used everything is ok
            daddiu      $t1, $t1, PAGE_TABLE_ENT_SIZE
            beqz        $a0, 2f                     # if un-used must set to used

            cfromptr    $c7, $kdc, $a0              # otherwise go into inner loop
            bne         $t1, $t8, l2_loop_start
            dli         $t9, (PAGE_TABLE_ENT_PER_TABLE << PAGE_TABLE_ENT_BITS) - PAGE_TABLE_ENT_SIZE # INCLUSIVE
            EXTRACT_AND_SCALE_L2 $a3, $t9, PAGE_TABLE_ENT_BITS
            l2_loop_start:
                cincoffset  $c8, $c7, $t2
l2_retry:       clld        $a0, $c8
                beq         $a0, $a4, l2_retry_yield
                nop
                beq         $a0, $a1, l2_loop_foot  # if used everything is ok, otherwise try set to used
                nop
                bnez        $a0, revoke_er          # A mapping still exists. Error.
                SET_ER(REVOKE_ER_HAS_ENTRY)
                cscd        $a0, $a1, $c8          # try set to used
                beqz        $a0, l2_retry
                nop

            l2_loop_foot:
                bne         $t2, $t9, l2_loop_start # t9 is the last INCLUSIVE
                daddiu      $t2, $t2, PAGE_TABLE_ENT_SIZE
            l2_loop_end:
                b           l1_loop_footer
                nop

        2:
            SET_ER(REVOKE_ER_L1_SKIP_START)
            bnez        $t2, revoke_er             # check we arn't skipping the start
            daddiu      $t2, $t0, -PAGE_TABLE_ENT_SIZE
            dsll        $t2, $t2, (UNTRANSLATED_BITS + L2_BITS + L1_BITS - PAGE_TABLE_ENT_BITS)
            dsll        $a0, $t1, (UNTRANSLATED_BITS + L2_BITS - PAGE_TABLE_ENT_BITS)
            daddu       $a0, $a0, $t2
            daddiu      $a0, $a0, (-UNTRANSLATED_PAGE_SIZE) # This is the inclusive upper bound we would be freeing
            sltu        $a0, $a3, $a0              # when the address is greater than it should be, we have an error
            bnez        $a0, revoke_er        # the last page must have an explicit entry
            SET_ER(REVOKE_ER_L1_SKIP_END)

            cscd        $a0, $a1, $c8         # if l1 was un-used set to used here
            beqz        $a0, l1_retry         # rety if l1 entry has changed
            daddiu      $t1, $t1, -PAGE_TABLE_ENT_SIZE
            daddiu      $t1, $t1, PAGE_TABLE_ENT_SIZE
        l1_loop_footer:
            bne         $t1, $t8, l1_loop_start
            dli         $t2, 0
        l1_loop_end:
            b           l0_loop_footer
            nop

    1:
        SET_ER(REVOKE_ER_L0_SKIP_START)
        dsll        $a0, $t0, (UNTRANSLATED_BITS + L2_BITS + L1_BITS - PAGE_TABLE_ENT_BITS)
        daddiu      $a0, $a0, (-UNTRANSLATED_PAGE_SIZE) # This is the inclusive upper bound we would be freeing
        sltu        $a0, $a3, $a0              # when the address is greater than it should be, we have an error
        bnez        $a0, revoke_er        # the last page must have an explicit entry
        daddu       $a0, $t1, $t2
        bnez        $a0, revoke_er             # the first page must have an explicit entry, unless aligned well
        SET_ER(REVOKE_ER_L0_SKIP_END)

        cscd        $a0, $a1, $c8
        beqz        $a0, l0_retry
        daddiu      $t0, -PAGE_TABLE_ENT_SIZE
        daddiu      $t0, $t0, PAGE_TABLE_ENT_SIZE

    l0_loop_footer:
        dli         $t1, 0
        bne         $t0, $t3, l0_loop_start
        dli         $t2, 0
    l0_loop_end:

    # Make exclusive again
    daddiu      $a3, $a3, UNTRANSLATED_PAGE_SIZE

    # We are now revoking. No errors from now on.

    dli         $a0, PHY_MEM_START_CACHED          #a0 is the start of memory
    dli         $a1, PHY_MEM_END_CACHED            #a1 the end

    # Now we have set revoking
    dli          $t0, DEF_DATA_PERMS

    dsubu        $a1, $a1, $a0
    beqz        $a1, revoke_loop_end
    csetoffset  $c5, $kdc, $a0
    dli         $t3, PHY_PAGE_SIZE
    dli         $t8, 0                      # t8 is our current byte index

revoke_loop_start:  # We will now revoke every capability with a2 <= base < a3.

    andi        $t1, $t8, (PHY_PAGE_SIZE-1)                                         # get in phy page offset
    bnez        $t1, 1f                                                             # if non zero keep scanning
    dsrl        $t1, $t8, (PHY_PAGE_SIZE_BITS - PHY_PAGE_ENTRY_SIZE_BITS)
    dli         $t2, phy_page_table
    daddu       $t1, $t1, $t2                                                       # this should be the index of
                                                                                    # a page entry

    # state of page can change concurrently - but it will only make us scan too much (not too little) #
    cincoffset  $c8, $idc, $t1

# FIXME: this is a potential deadlock
# FIXME: maybe after some point assume we should scan it
# FIXME: or make transaction its own bit, not just a state
    dli         $t0, page_transaction
2:
    cllw        $t2, $c8
    beq         $t2, $t0, 2b                           # If transacting wait till we can find out the type
    cld         $t1, $zero, PHY_PAGE_OFFSET_len($c8)    # Get length in load link. if non zero use last state
    cscw        $t0, $t2, $c8                          # store to make sure state/length consistent
    beqz        $t0, 2b
    dli         $t0, page_transaction

    movn        $t9, $t2, $t1                          # state only valid if length non zero. Use last state otherwise

    # only bother scanning nano owned, mapped and system owned #
    li          $t0, 1
    dsllv       $t0, $t0, $t9
    andi        $t0, $t0, (1 << page_nano_owned) | (1 << page_system_owned) | ( 1 << page_mapped)
    bnez        $t0, 1f
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
    cincoffset  $c5, $c5, CAP_SIZE

revoke_loop_end:


    # with multicore this will require a sync. This will force a switch to ourselves which will clear registers #
    # First check if we have had a content switch or not. We can force ourselves by calling
    # context_switch as a sub-routine.
    cld         $t0, $zero, since_switched($kr1c)
    bnez        $t0, revoke_check_switched

#   Call switch to ourselves.
    clc         $c3, $zero, current_context($idc)
    cincoffset  $c5, $kdc, CONTEXT_TYPE
    cseal       $c3, $c3, $c5
    cmove       $c6, $c17
    cmove       $c7, $c18
    li          $t0, 0
    cgetpcc     $c17     # this is mocking up a capability branch and link
    beqz        $t0, 1f # FIXME wrong way around?
    cmove       $c18, $idc
    b           context_switch
    li          $t0, 1
1:  cmove       $c17, $c6
    cmove       $c18, $c7

revoke_check_switched:

#ifdef SMP_ENABLED

    # TODO we should signal others with a dummy signal to force a switch
    # Check others. Currently we just yield.
    dli         $t0, (context_array_size) * (SMP_CORES-1)
    sync
1:  YIELD
2:  cld         $t1, $t0, since_switched($idc)
    beqz        $t1, 1b
    nop
    bnez        $t0, 2b
    daddiu      $t0, $t0, -(context_array_size)
#endif


revoke_restore:

    # Allow any cap writes again #
#ifndef CHERI_LEGACY_COMPAT
    dmtc0       $zero, MIPS_CP0_REG_REVOKE, MIPS_CP0_REG_REVOKE_PERMS
    dmtc0       $zero, MIPS_CP0_REG_REVOKE, MIPS_CP0_REG_REVOKE_BOUND
    dmtc0       $zero, MIPS_CP0_REG_REVOKE, MIPS_CP0_REG_REVOKE_BASE
#endif

    # Mark vtable entries as free

    dli         $t0, top_virt_page
    cincoffset  $c5, $idc, $t0              # c5 is the top level table
                                            # c6 is the L1 table. c7 the l2
                                            # c8 is for load link
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
        cincoffset  $c8, $c5, $t0
clear_loop_l0_start_retry:
        clld        $a0, $c8
        bne         $a0, $a1, 1f    # if used set to free, otherwise recurse
        daddiu      $t0, $t0, PAGE_TABLE_ENT_SIZE

        # Error check edge cases #
        dsll        $a0, $t0, (UNTRANSLATED_BITS + L2_BITS + L1_BITS - PAGE_TABLE_ENT_BITS)
        daddiu      $a0, $a0, (-UNTRANSLATED_PAGE_SIZE) # This is the inclusive upper bound we would be freeing
        sltu        $a0, $a3, $a0              # if the address is greater than it should be, we have an error
        SET_ER(REVOKE_ER_L0_SKIP_START2)
        bnez        $a0, revoke_er        # the last page must have an explicit entry
        daddu       $a0, $t1, $t2
        bnez        $a0, revoke_er             # the first page must have an explicit entry, unless aligned well
                                               # when t1 and t2 are zero, this is not the first page, or its aligned
        SET_ER(REVOKE_ER_L0_SKIP_END2)

        cscd        $a0, $zero, $c8                 # set to free
        beqz        $a0, clear_loop_l0_start_retry
        daddiu      $t0, $t0, -PAGE_TABLE_ENT_SIZE
        daddiu      $t0, $t0, PAGE_TABLE_ENT_SIZE
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
            cincoffset  $c8, $c6, $t1
clear_loop_l1_start_retry:
            clld        $a0, $c8
            bne         $a0, $a1, 1f    # if used set to free, otherwise recurse
            daddiu      $t1, $t1, PAGE_TABLE_ENT_SIZE

            # Error check edge cases #
            SET_ER(REVOKE_ER_L1_SKIP_START2)
            bnez        $t2, revoke_er             # check we arn't skipping the start
            daddiu      $t2, $t0, -PAGE_TABLE_ENT_SIZE
            dsll        $t2, $t2, (UNTRANSLATED_BITS + L2_BITS + L1_BITS - PAGE_TABLE_ENT_BITS)
            dsll        $a0, $t1, (UNTRANSLATED_BITS + L2_BITS - PAGE_TABLE_ENT_BITS)
            daddu       $a0, $a0, $t2
            daddiu      $a0, $a0, (-UNTRANSLATED_PAGE_SIZE) # This is the inclusive upper bound we would be freeing
            sltu        $a0, $a3, $a0              # if the address is greater than it should be, we have an error
            bnez        $a0, revoke_er        # the last page must have an explicit entry
            SET_ER(REVOKE_ER_L1_SKIP_END2)

            cscd        $a0, $zero, $c8
            beqz        $a0, clear_loop_l1_start_retry
            daddiu      $t1, $t1, -PAGE_TABLE_ENT_SIZE
            daddiu      $t1, $t1, PAGE_TABLE_ENT_SIZE

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
                csd         $zero, $t2, (-PAGE_TABLE_ENT_SIZE)($c7) # We checked these in the first loop
                b           2b
                nop

            clear_loop_l2_end:

        clear_loop_l1_end:

    clear_loop_l0_end:

    # Make exclusive again

    daddiu      $a3, $a3, (UNTRANSLATED_PAGE_SIZE)
    dsubu       $a4, $a3, $a2

    # Make a new reservation here now. This will cause a fault but the vtables should be available again #

    cincoffset  $c4, $kdc, RES_TYPE              # create unsealing cap

    dli         $t0, RES_META_SIZE
    csetoffset  $c3, $kdc, $a2
    csetboundsexact  $c3, $c3, $t0
    sub         $a4, $a4, $t0               #some space for metadata

    dli         $t1, DEF_DATA_PERMS
    candperm    $c3, $c3, $t1

    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3)       # set state to open
    csd         $zero, $zero, (RES_PID_OFFSET)($c3)                   # set parent to 0
    csd         $a4, $zero, RES_LENGTH_OFFSET($c3)                    # set length

    cseal       $c3, $c3, $c4               # seal new reservation

    sync

    csd         $zero, $zero, revoke_state($idc) # Set revoke state to 0

    ctoint      $s0, $c9
    cclearlo    EN5(c4, c5, c6, c7, c8)
    CRETURN

revoke_er:
    inttoc      $c3, $s0
    ctoint      $s0, $c9
    cclearlo    EN5(c4, c5, c6, c7, c8)
    CRETURN


################################################### (safe)
# res_t  rescap_split(capability res, size_t size)
.global rescap_split
rescap_split:
###################################################
    andi        $t0, $a0, (RES_META_SIZE-1) # check alignment
    bnez        $t0, split_er
    cgetoffset  $t0, $c3
    bnez        $t0, split_er               # can't split subfields

    cincoffset  $c4, $kdc, RES_TYPE         # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res

1:  LOADL_RES_STATE $t0, $c3                # get state
    bnez        $t0, split_er               # reservation must be open
    li          $t0, res_trans
    STOREC_RES_STATE $t0, $t0, $c3          # set transacting to avoid race
    beqz        $t0, 1b

    cld         $t1, $zero, (RES_LENGTH_OFFSET)($c3) # get length
    beqz        $t1, split_er_backout       # length must be non zero

    daddiu      $a1, $t1, -(RES_META_SIZE)  # length minus new metadata space
    sltu        $t0, $a1, $a0
    bnez        $t0, split_er_backout       # size arg too large

    # At this point we can no longer error

    dsubu       $a1, $a1, $a0                   # size of remainder
    cld         $t0, $zero, (RES_PID_OFFSET)($c3)       # get pid
    csd         $a0, $zero, (RES_LENGTH_OFFSET)($c3)    # set length for old res

    # We can now allow the original to be used again
    sync
    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3)

    dli         $t8, RES_META_SIZE

    cgetbase    $t3, $c3
    daddiu      $t3, $t3, RES_META_SIZE
    daddu       $t3, $t3, $a0               # start of next reservation

    dli         $t1, DEF_DATA_PERMS

    csetoffset  $c3, $kdc, $t3              # Create new meta node
    csetboundsexact  $c3, $c3, $t8
    candperm    $c3, $c3, $t1

    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3)     # set state of new reservation to open
    csd         $a1, $zero, (RES_LENGTH_OFFSET)($c3)        # set length of new reservation
    csd         $t0, $zero, (RES_PID_OFFSET)($c3)           # set parent

    cseal        $c3, $c3, $c4
    cclearlo    EN1(c4)
    CRETURN

split_er_backout:
    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3)
split_er:
    cclearlo    EN2(c3, c4)
    CRETURN


############################################# (safe)
# res_t rescap_merge(res_t res1, res_t res2)
.global rescap_merge
rescap_merge:
#############################################
    ceq         $t0, $c3, $c4
    bnez        $t0, merge_er               # naughty people may try trip us up merging a reservation with itself
    cincoffset  $c5, $kdc, RES_TYPE         # create unsealing cap
    cunseal     $c3, $c3, $c5
    cunseal     $c4, $c4, $c5               # unseal both arguments

    cgetoffset  $t1, $c3                    # can't split subfields
    bnez        $t1, merge_er
    cgetoffset  $t1, $c4
    bnez        $t1, merge_er

    dli         $t3, res_taken

1:  LOADL_RES_STATE $t1, $c3
    bne         $t3, $t1, merge_er          # check first is taken
    li          $t1, res_trans
    STOREC_RES_STATE $t1, $t1, $c3
    beqz        $t1, 1b

2:  LOADL_RES_STATE $t1, $c4
    bne         $t3, $t1, merge_er_backout1 # check first is taken
    li          $t1, res_trans
    STOREC_RES_STATE $t1, $t1, $c4
    beqz        $t1, 2b

    cld         $t0, $zero, (RES_PID_OFFSET)($c3) # get depth of first
    cld         $t1, $zero, (RES_PID_OFFSET)($c4) # get depth of second
    bne         $t0, $t1, merge_er_backout2          # must have same depth

    cld         $t0, $zero, RES_LENGTH_OFFSET($c3)   # get first length
    cld         $t1, $zero, RES_LENGTH_OFFSET($c4)   # get second length
    daddiu      $t0, RES_META_SIZE
    cincoffset  $c6, $c3, $t0               # c6 should point to the next reservation
    cne         $t2, $c6, $c4               # if not equal then we are not merging adjacent blocks
    bnez        $t2, merge_er_backout2

# From here on we can be confident of no error

    daddu        $t0, $t0, $t1               # this is the total new length
    li          $t1, res_merged
    csd         $t0, $zero, RES_LENGTH_OFFSET($c3)      # store back in the field of the lower address reservation a new length.
    sync
    STORE_RES_STATE $t1, $zero, RES_STATE_OFFSET($c4)   # set higher address reservation to merged
    STORE_RES_STATE $t3, $zero, RES_STATE_OFFSET($c3)   # set lower address back to taken
    cseal       $c3, $c3, $c5               # although the caller can just use the first argument, its nice to return it
    cclearlo    EN3(c4, c5, c6)
    CRETURN

merge_er_backout2:
    STORE_RES_STATE $t3, $zero, RES_STATE_OFFSET($c4)     # set higher address reservation back to open
merge_er_backout1:
    STORE_RES_STATE $t3, $zero, RES_STATE_OFFSET($c3)     # set lower address back to open
merge_er:
    cclearlo    EN4(c3, c4, c5, c6)
    CRETURN

################################# (safe)
# res_t rescap_parent(res_t res)
.global rescap_parent
rescap_parent:
#################################
    cincoffset  $c4, $kdc, RES_TYPE              # create unsealing cap
    cunseal     $c3, $c3, $c4               # unseal res
    cgetoffset  $t0, $c3
    bnez        $t0, parent_er              # must not be a sub field

    dli          $t2, res_taken
    dli          $t0, RES_SIZE_NOT_FIELD
1:  LOADL_RES_STATE $t1, $c3                # get state
    bnez        $t1, parent_er              # reservation must be open
    cld         $t8, $zero, (RES_LENGTH_OFFSET)($c3) # get length
    beqz        $t8, parent_er              # empty reservation will not have enough space
    nop # Had the store here. Doesn't work on FPGA.
    STOREC_RES_STATE $t1, $t2, $c3          # set state field for parent
    beqz        $t1, 1b
    csb         $t0, $zero, RES_SUBFIELD_SIZE_OFFSET($c3) # fine to do to anything open

    cgetbase    $t0, $c3                    # parent base is id for child
    dli         $a0, RES_META_SIZE

    daddiu      $t2, $t8, (-RES_META_SIZE)  # length of child

    csetoffset  $c3, $kdc, $t0              # create new res node
    cincoffset  $c3, $c3, $a0
    csetboundsexact  $c3, $c3, $a0
    dli         $t3, DEF_DATA_PERMS
    candperm    $c3, $c3, $t3

    STORE_RES_STATE $zero, $zero, RES_STATE_OFFSET($c3) # set state for child
    csd         $t2, $zero, (RES_LENGTH_OFFSET)($c3)    # set length for child
    csd         $t0, $zero, (RES_PID_OFFSET)($c3)       # set depth for child

    cseal       $c3, $c3, $c4               # seal child for return

    cclearlo    EN1(c4)
    CRETURN

parent_er:
    cclearlo    EN2(c3, c4)
    CRETURN

# Notes: will lock the physical page, and atomically updates the vtable entry #
######################################################################## (safe)
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
    li          $a2, page_ptable_free

    cincoffset  $c5, $idc, $t0                          # c5 is out physical entry

2:  cllw        $t9, $c5                                # $t9 = state. Take out transaction lock.
    # t9 must be page_ptable_free or 0
    movz        $a2, $zero, $t9
    bne         $t9, $a2, create_ptable_er
    li          $a2, page_ptable_free
    cscw        $t1, $t8, $c5
    beqz        $t1, 2b
    nop

    cld         $t1, $zero, PHY_PAGE_OFFSET_len($c5)    # $t1 = len of record
    daddiu      $t1, $t1, -1
    bnez        $t1, create_ptable_er_restore_state     # $t1 must be 1
    nop

    dsll        $t0, $a0, PHY_PAGE_SIZE_BITS
    dli         $t1, PHY_MEM_START_CACHED
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
    csetboundsexact  $c3, $c3, $t1
    csetoffset  $c4, $kdc, $t2
    cseal       $c3, $c3, $c4



    cclearlo    EN4(c4,c17, c5, c6)
    CRETURN

create_ptable_er_restore_state:
    csw         $t9, $zero, 0($c5)

create_ptable_er:
    cclearlo    EN5(c3, c17, c4, c5, c7)
    CRETURN


# checked untranslated # l0_index # l1_index # l2_index # unchecked untranslated bits

########################################################### (safe)
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
    cnull       $c3

    csetoffset  $c3, $kdc, $t0
    dli         $t1, PAGE_TABLE_SIZE
    csetboundsexact  $c3, $c3, $t1
    cseal       $c3, $c3, $c4

    get_sub_table_end:
    cclearlo    EN1(c4)
    CRETURN

    get_sub_table_er:
    cclearlo    EN2(c3,c4)
    CRETURN

########################################################### (safe)
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
    CRETURN

# Notes: Locks the physical page, and atomically updates the vtable entry #
######################################################################################### (safe)
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
    bnez        $t9, create_mapping_er                  # must be un-used
    nop
    cscw        $t0, $t8, $c6                           # lock phy entry
    beqz        $t0, 2b

    cld         $t1, $zero, PHY_PAGE_OFFSET_len($c6)    # $t1 = len of record
    daddiu      $t1, $t1, -2
    bnez        $t1, create_mapping_er_restore_phy      # $t1 must be 2
    dsll        $t2, $a1, PAGE_TABLE_ENT_BITS           # t2 is offset of page entry to c3

    cincoffset  $c5, $c3, $t2                           # c5 is virtual entry
    dsll        $a0, $a0, PFN_SHIFT
    or          $a0, $a0, $a2

1:  clld        $t1, $c5                               # check -virtual- mapping is un-used
    bnez        $t1, create_mapping_er_restore_phy
    nop
    cscd        $t1, $a0, $c5                          # store EntryLo
    beqz        $t1, 1b

    dli         $t9, page_mapped

    # TODO add autharising cap
    # TODO add the checked top bits

create_mapping_er_restore_phy:
    csw         $t9, $zero, PHY_PAGE_OFFSET_status($c6)

create_mapping_er:
    cclearlo    EN4(c3,c4,c5,c6)
    CRETURN



###################################################### (safe)
# void free_mapping(ptable_t table, register_t index)
.global free_mapping
free_mapping:
######################################################

    cgettype    $t0, $c3
    daddu       $t1, $t0, -VTABLE_TYPE_L0

    sltiu       $t2, $t1, VTABLE_LEVELS
    beqz        $t2, free_er
    daddiu      $t1, $t1, -(VTABLE_LEVELS-1)             # t1 zero if last level

    csetoffset  $c4, $kdc, $t0

    sltiu       $t0, $a0, PAGE_TABLE_ENT_PER_TABLE
    beq         $t0, $zero, free_er                     # $a0 >= PAGE_TABLE_ENT_PER_TABLE

    cunseal     $c3, $c3, $c4
    dsll        $t2, $a0, PAGE_TABLE_ENT_BITS           # t2 is offset of page entry to c3

    cincoffset  $c3, $c3, $t2                           # c3 is the vtable entry

1:  dli         $t0, VTABLE_ENTRY_USED
    clld        $t3, $c3                                # t3 is the PFN/Physical adress of page
    beq         $t0, $t3, free_er                       # must not already be free
    li          $t8, VTABLE_ENTRY_TRAN                  # set to this while fiddling with physical pages
    cscd        $t8, $t8, $c3
    beqz        $t8, 1b
    dli         $t9, 2                                  # hoisted from a couple instructions below to fill slot

    beqz        $t3, free_to_used

    dli         $a3, page_dirty
    beqz        $t1, 2f
    dsrl        $t8, $t3, PFN_SHIFT                     # the last level has a PFN entry for the TLB
    dli         $a3, page_ptable_free
    dli         $t9, 1
    dsll        $t8, $t3, TOP_ADDR_BITS                 # other levels have physical indexs. first zero top.
    dsrl        $t8, $t8, TOP_ADDR_BITS + PHY_PAGE_SIZE_BITS # then shift down to get a pagen
2:
    # t8 is the physical pagen. t9 has how many pages we will be freeing. a3 the type to set to.
    # t0 is constant VTABLE_ENTRY_USED. t3 is old PFN/PHY addr for restore.

    dsll        $t1, $t8, PHY_PAGE_ENTRY_SIZE_BITS
    cincoffset  $c4, $idc, $t1
    dli         $t1, phy_page_table
    cincoffset  $c4, $c4, $t1                           # c4 is our phy entry


    dli         $t2, page_transaction

3:  clld        $t1, $c4                                # t1 is the state of the phy page (might be transacting)
    beq         $t1, $t2, 3b                            # TODO think about whether the loop here is appropriate?
                                                        # The state will eventually be correct as
                                                        # we know we were mapping to it
    li          $t2, page_transaction
    cscd        $t2, $t2, $c4                           # set p-entry to transacting
    beqz        $t2, 3b


    cld         $t0, $zero, PHY_PAGE_OFFSET_len($c4)     # $t0 is the length
    beq         $t0, $t9, 4f                             # if length is wrong back out of changing the vtable entry

    move        $t0, $t3
    b           free_to_used                             # will set v-entry to what it was before
    csd         $t1, $zero, PHY_PAGE_OFFSET_status($c4)  # set p-entry back to whatever it was

4:

    li          $t0, 1
    csd         $t0, $zero, need_shootdown($idc)        # only shootdown when we try to clear a page
    sync                                                # need shootdown must be seen if the page is seen as being dirty

# At this point we might be allowed to use the page again. TLB flush must have already happened
    csw         $a3, $zero, PHY_PAGE_OFFSET_status($c4)    # store to update phy page status
    li          $t0, VTABLE_ENTRY_USED
free_to_used:
    csd         $t0, $zero, 0($c3)                        # store -1 in vtable to mean 'used'

free_er:
    cclearlo    EN2(c3, c4)
    CRETURN



#struct foundation_metadata {
#    found_id_t id;
#    capability data;
#    capability entrys[n_entries];
#};

########################################################################################### (safe)
# entry_t foundation_create(res_t res, size_t image_size, capability image,
#                           size_t entry0, size_t n_entries, register_t public)
.global foundation_create
foundation_create:
###########################################################################################

    beqz        $a2, found_er                       # must have at least 1 entry slot
    andi        $t0, $a0, (REG_SIZE-1)
    bnez        $t0, found_er                       # make sure we copying a multiple of a double
    cgetbase    $t0, $c4
    cgetoffset  $t1, $c4
    daddu       $t0, $t0, $t1
    andi        $t0, $t0, (REG_SIZE-1)
    bnez        $t0, found_er                       # check source alignment
    nop

    cmove       $c13, $c4                           # save image

    SUBROUTINE_TAKE

    cbtu        $c3, found_er

    # check res size
    cgetlen     $t0, $c3

    # calculate meta size
    dsll        $t1, $a2, CAP_SIZE_BITS
    daddiu      $t1, $t1, (FOUNDATION_ID_SIZE + CAP_SIZE)

    # check meta+image <= res size
    daddu       $t2, $t1, $a0
    sltu        $t3, $t0, $t2
    bnez        $t3, found_er

    # store other id fields
    csd         $a0, $zero, FOUNDATION_ID_LEN_OFFSET($c4)                   # id length field
    csd         $a1, $zero, FOUNDATION_ID_E0_OFFSET($c4)                    # id entry0 field
    csd         $a2, $zero, FOUNDATION_ID_NENT_OFFSET($c4)                  # if nentries field

    cmove       $c14, $c4                           # save c4 for later

    # Make sure this is null for a public foundation
    csc         $cnull, $zero, FOUNDATION_META_DATA_OFFSET($c14) # store data for invocation

    cincoffset  $c4, $c14, $t1                       # c4 is the start of the user code/data
    dli         $t0, DEF_DATA_PERMS
    csetboundsexact  $c4, $c4, $a0                       # c4 is users data
    # store data we will get on entry
    bnez        $a3, 1f
    candperm    $c4, $c4, $t0
    csc         $c4, $zero, FOUNDATION_META_DATA_OFFSET($c14) # store data for invocation

1:

    # create first entry point
    cincoffset  $c3, $c3, $t1                       # skip over metadata
    csetboundsexact  $c3, $c3, $a0                       # correctly bounded code pointer
    csetoffset  $c3, $c3, $a1                       # correctly offset entry

    csc         $c3, $zero, FOUNDATION_META_ENTRY_VECTOR_OFFSET($c14) # store first entry trampoline

    # create return token
    csetboundsexact  $c15, $c14, $t1                       # we can set offset to 0 to get metadata
    dli         $t3, FOUNDATION_META_ENTRY_VECTOR_OFFSET
    csetoffset  $c15, $c15, $t3                       # entry points to pcc to load in trampoline

    dli         $t3, FOUND_ENTRY_TYPE
    csetoffset  $c3, $kdc, $t3
    cseal       $c15, $c15, $c3

    # memcpy into image calculating sha256 as we go. c3 = source. c4 = dest. a0 = size.
    # Normal ABI, but saves c14 to c16 for us (how kind)
    cmove       $c3, $c13
    cmove       $c16, $c17          # save c17

    dla         $t0, sha256_copy    # jump to sha256 routine
    cgetpccsetoffset $c12, $t0
    cjalr       $c12, $c17
    nop

    # restore c17
    cmove       $c17, $c16

    # store hash
    csd         $v0, $zero, 0($c14)
    csd         $v1, $zero, 8($c14)
    csd         $t0, $zero, 16($c14)
    csd         $t1, $zero, 24($c14)

    # get back return value we calculated before the hash
    cmove       $c3, $c15

    cclearlo    EN11(c4,c5,c6,c7,c8,c9,c12,c13,c14,c15,c16)
    CRETURN

    found_er:
    cclearlo    EN12(c3,c4,c5,c6,c7,c8,c9,c12,c13,c14,c15,c16)
    CRETURN


########################################################################################### (safe)
# void foundation_enter(entry_t entry)
.global foundation_enter
foundation_enter:
###########################################################################################

    dli         $t3, FOUND_ENTRY_TYPE
    csetoffset  $c4, $kdc, $t3
    cunseal     $c3, $c3, $c4

    clc         $c12, $zero, 0($c3)                     # get entry pcc
    csetoffset  $c3, $c3, $zero
    clc         $c4, $zero, current_context($kr1c)       # get current context
    clc         $idc, $zero, FOUNDATION_ID_SIZE($c3)    # get entry data

    csc         $c3, $zero, CONTEXT_OFFSET_FOUND($c4)     # set current context to point to id of foundation

    cjr         $c12
    cclearlo    EN2(c3,c4)

########################################################################################### (safe)
# capability foundation_entry_expose(entry_t entry)
.global foundation_entry_expose
foundation_entry_expose:
###########################################################################################

    cincoffset  $c4, $kdc, FOUND_ENTRY_TYPE
    cunseal     $c3, $c3, $c4                           # unseal foundation
    csetoffset  $c4, $c3, $zero
    clc         $c4, $zero, (FOUNDATION_META_DATA_OFFSET)($c4)   # check is public
    cbts        $c4, nano_er
    clc         $c3, $zero, 0($c3)                      # get pcc
    cclearlo    EN1(c4)
    CRETURN

########################################################################################### (safe)
# found_id_t* foundation_entry_get_id(entry_t entry)
.global foundation_entry_get_id
foundation_entry_get_id:
###########################################################################################

    cincoffset  $c4, $kdc, FOUND_ENTRY_TYPE
    cunseal     $c3, $c3, $c4                           # unseal foundation
    csetoffset  $c3, $c3, $zero
    li          $t1, FOUNDATION_ID_SIZE
    csetboundsexact  $c3, $c3, $t1                      # set bounds
    dli         $t0, Perm_Load
    candperm    $c3, $c3, $t0                           # read only
    cclearlo    EN1(c4)
    CRETURN

########################################################################################### (safe)
# void foundation_exit(void)
.global foundation_exit
foundation_exit:
###########################################################################################

    clc         $c3, $zero, current_context($kr1c)       # get current context
    csc         $cnull, $zero, CONTEXT_OFFSET_FOUND($c3)
    cclearlo    EN2(c3,c4)
    CRETURN


########################################################################################### (safe)
# entry_t foundation_new_entry(size_t eid, capability at)
.global foundation_new_entry
foundation_new_entry:
###########################################################################################

    # Get found struct from current context
    clc         $c4, $zero, current_context($kr1c)
    clc         $c4, $zero, CONTEXT_OFFSET_FOUND($c4)

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
    CRETURN

####################################### (safe)
# found_id_t* foundation_get_id (void)
.global foundation_get_id
foundation_get_id:
#######################################

    # Get found struct from current context

    clc         $c3, $zero, current_context($kr1c)
    clc         $c3, $zero, CONTEXT_OFFSET_FOUND($c3)
    cbtu        $c3, id_none

    # make cert for current foundation
    dli         $t0, FOUNDATION_ID_SIZE
    csetboundsexact  $c3, $c3, $t0
    dli         $t0, Perm_Load
    candperm    $c3, $c3, $t0

id_none:
    CRETURN

########################################################################################### (safe)
# cert_t rescap_take_cert(res_t res, cap_pair* out, register_t user_perms)
.global rescap_take_cert
rescap_take_cert:
###########################################################################################

    # get current foundation
    clc         $c5, $zero, current_context($kr1c)
    clc         $c5, $zero, CONTEXT_OFFSET_FOUND($c5)
    cbtu        $c5, take_cert_er

    # make cert for current foundation
    dli         $t0, FOUNDATION_ID_SIZE
    csetboundsexact  $c5, $c5, $t0
    dli         $t0, Perm_Load
    candperm    $c5, $c5, $t0

    b           rescap_take_locked_cert_shared
    dli         $a1, FOUND_CERT_TYPE

########################################################################################### (safe)
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
    csetboundsexact  $c6, $c6, $t0
    csetboundsexact  $c3, $c3, $t1
    csetboundsexact   $c4, $c4, $t1

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
    CRETURN

take_cert_er:
    cclearlo    EN8(c3,c4,c5,c6,c7,c13,c14,c15)
    CRETURN

########################################################################################### (safe)
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
    CRETURN


########################################################################################### (safe)
# void rescap_unlock(locked_t locked, cap_pair* out)
.global rescap_unlock
rescap_unlock:
###########################################################################################

    # unlock
    dli         $t0, FOUND_LOCKED_TYPE
    csetoffset  $c5, $kdc, $t0
    cunseal     $c3, $c3, $c5

    # get current foundation
    clc         $c5, $zero, current_context($kr1c)
    clc         $c5, $zero, CONTEXT_OFFSET_FOUND($c5)
    cbtu        $c5, 1f

    # convert to same form as used for cert earlier
    dli         $t0, FOUNDATION_ID_SIZE
    csetboundsexact  $c5, $c5, $t0
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
    CRETURN

1:
    cclearlo    EN4(c3,c4,c5,c6)
    CRETURN


################################### (safe)
# tres_t tres_get(register_t type)
.global tres_get
tres_get:
###################################

    slti    $t0, $a0, NANO_TYPES    # 1 if reserved nano type
    bnez    $t0, nano_er

    li      $t0, DEF_SEALING_PERMS
    li      $t1, TRES_TYPE

    csetoffset $c3, $kdc, $a0
    csetoffset $c4, $kdc, $t1

    candperm $c3, $c3, $t0
    li       $t0, 1
    csetboundsexact $c3, $c3, $t0
    cseal   $c3, $c3, $c4

    cclearlo    EN1(c4)
    CRETURN

################################### (safe)
# sealing_cap tres_take(tres_t tres)
.global tres_take
tres_take:
###################################

    cincoffset  $c4, $kdc, TRES_TYPE
    cunseal     $c3, $c3, $c4

    cgetbase    $t0, $c3
    daddiu      $t0, $t0, -NANO_TYPES

    dli         $t2, 1
    andi        $t1, $t0, 0x7               # get index into byte
    dsll        $t1, $t2, $t1               # get mask for bit in byte
    dsrl        $t0, $t0, 3                 # get index into bitfield

    daddiu      $t0, $t0, tres_bitfield
    cincoffset  $c4, $idc, $t0               # c4 is capability to byte

1:  cllb        $t0, $c4
    and         $t2, $t1, $t0               # extract bit
    bnez        $t2, nano_er                # bit should be 0
    or          $t0, $t0, $t1               # set bit to 1
    cscb        $t0, $t0, $c4
    beqz        $t0, 1b                     # store cond fail, try this again
    nop

    cclearlo    EN1(c4)
    CRETURN

############################################################ (safe)
# capability tres_revoke(register_t start, register_t end)
.global tres_revoke
tres_revoke:
############################################################

#  TODO:

    CRETURN

# Gets a base for PIC config (v0), and maps an interrupt number to a PIC number (v1). Returns via at. clobbers t0/t1
interrupt_common:
    move    $at, $ra
    move    $ra, $t0

    sltiu   $t0, $a0, SMP_CORES
    beqz    $t0, mask_er
    sltiu   $t0, $a1, INTERRUPTS_N
    beqz    $t0, mask_er
    sltu    $a2, $zero, $a2                         # postive->1, 0->0

#ifdef HARDWARE_qemu
    jr      $at
    nop
#else
    # Get a config base for this cpu
    dli     $v0, PHY_MEM_START_UNCACHED + PIC_CONFIG_START # v0 is CONFIG_START
    dsll    $t0, $a0, PIC_CONFIG_SZ_LOG_2
    daddu   $v0, $v0, $t0                           # CONFIG_BASE(cpu)

    # System: [0,31] -> [0, 31], [32,63] -> [64,95]
    # Nano software used: [96,127]

    # Map onto pic registers
    sltiu   $t0, $a1, 32
    daddiu  $v1, $a1, 32                            # the software interrupts
    jr      $at
    movn    $v1, $a1, $t0                           # v1 = (a1 if (a1 < 32) else a1 + 32)
#endif


############################################################ (safe)
# void interrupts_mask(uint8_t cpu, register_t n, int enable)
.global interrupts_mask
interrupts_mask:
############################################################

    move    $t0, $ra
    bal     interrupt_common
    nop

#ifdef HARDWARE_qemu

    # Ignores CPU. Must pin things to 0.

    daddiu  $a1, $a1, MIPS_CP0_STATUS_IM_SHIFT + 2
    li      $t0, 1
    dsll    $t0, $t0, $a1                           # bit in correct position to mask


    dmfc0   $t1, MIPS_CP0_REG_STATUS
    or      $t2, $t1, $t0                           # t2 bit set
    not     $t0, $t0
    and     $t1, $t1, $t0                           # t1 bit unset
    movn    $t1, $t2, $a2                           # t1 = (t2 if enable else t1)

    dmtc0   $t1, MIPS_CP0_REG_STATUS

#else // qemu

    dsll    $v1, $v1, 3                             # each is 8 bytes
    daddu   $v0, $v0, $v1                           # CONFIG_X(cpu, n)

    dsll    $a2, $a2, PIC_CONFIG_OFFSET_E
    daddiu  $a2, $a2, STATUS_BIT_TO_BERI_IRQ(INTERRUPTS_SYSTEM_N)
    csd     $a2, $v0, 0($kdc)

#endif

mask_er:
    CRETURN

############################################################ (safe)
# void interrupts_soft_set(uint8_t cpu, register_t n, int enable)
.global interrupts_soft_set
interrupts_soft_set:
############################################################

#ifdef HARDWARE_qemu
    CRETURN
#else // qemu

    move    $t0, $ra
    bal     interrupt_common
    nop
    daddiu  $v0, $v0, (8 * 1024) + 128      # Set
    daddiu  $t0, $v0, 128                   # Clear
    movz    $v0, $t0, $a2                   # v0 = (set if enable else clear)

# FIXME pretty sure I cant use byte access. Word or double word seem to be allowed.
    # low 3 bits are byte index. Upper bits index into bytes.
    andi    $t0, $v1, 0x0b111               # byte index
    li      $t1, 1
    sll     $t0, $t1, $t0                   # a '1' in the correct bit position of the byte
    srl     $t1, $v1, 3
    daddu   $v0, $v0, $t1
    csb     $t0, $v0, 0($kdc)
    CRETURN

#endif

############################################################ (safe)
# uint64_t interrupts_get(uint8_t cpu)
.global interrupts_get
interrupts_get:
############################################################

#ifdef HARDWARE_qemu
    # Ignores CPU. Must pin to 0.
    dmfc0   $v0, MIPS_CP0_REG_CAUSE
    dsrl    $v0, $v0, (MIPS_CP0_STATUS_IM_SHIFT+2)
    andi    $v0, $v0, ((1 << INTERRUPTS_N_HW)-1)
    CRETURN

#else // qemu
    move    $t0, $ra
    bal     interrupt_common
    li      $a1, 0                  # Dummy, we will be getting many of these
    daddiu  $v0, $v0, (8 * 1024)
    # System: [0,31] -> [0, 31], [32,63] -> [64,95]
    # Nano software used: [96,127]
    clw     $t0, $v0, 4($kdc)      # [0,31] <- [0, 31]
    clw     $t1, $v0, ((64/8)+4)($kdc) # [32,63] <- [64,95]
    dsll    $t1, $t1, 32
    daddu   $v0, $t0, $t1
    CRETURN
#endif

############################################################ (safe)
# void exception_subscribe(void)
.global exception_subscribe
exception_subscribe:
############################################################

    # Just set the user_handle bit in CONTEXT_EX_STATE
    clcbi   $c3, current_context($kr1c)
    cld     $t0, $zero, (CONTEXT_OFFSET_EX_STATE)($c3)
    ori     $t0, $t0, EX_STATE_UH
    csd     $t0, $zero, (CONTEXT_OFFSET_EX_STATE)($c3)
    cclearlo EN1(c3)
    CRETURN

# WARN: Trying to guarentee this only clobers c1/v0/v1 so exception handlers can be fast #
############################################################ (safe)
# user_exception_cause_t exception_getcause(void)
.global
exception_getcause:
############################################################

    clcbi   $c1, current_context($kr1c)
    cld     $v0, $zero, (CONTEXT_OFFSET_CAUSE)($c1)
    cld     $v1, $zero, (CONTEXT_OFFSET_CCAUSE)($c1)
    cclearlo EN1(c1)
    CRETURN

############################################################  (safe)
# void exception_return(void)
.global exception_return
exception_return:
############################################################

    clcbi       $c1, current_context($kr1c)
    csd         $zero, $zero, CONTEXT_OFFSET_CAUSE($c1)

############################################################ (safe)
# void, exception_replay(void)
.global exception_replay
exception_replay:
############################################################
    inttoc      $c1, $t1        # Got to save t1 # restart point for vmem exception
    SET_EXL     $t1             # is 3 instructions
    ctoint      $t1, $c1
    cgetpcc     $epcc
    cincoffset  $epcc, $epcc, -4 * (5)
    SYNC_EPCC
    QEMU_BUG_FIX $k1

    clcbi       $kr2c, current_context($kr1c)              # Our current context

    # Force TLB exceptions. (from context)
    cld         $k1, $zero, (-INC_IM_MAX)($kr2c)
    cld         $k1, $zero, (CONTEXT_SIZE-REG_SIZE-INC_IM_MAX)($kr2c)

    clcbi       $c1, CONTEXT_OFFSET_EX_SAVED_IDC($kr2c)    # The idc to swap with

    # Force TLB exceptions. (from ctlp)
    cld         $k1, $zero, CTLP_OFFSET_EX_PCC($c1)
    cld         $k1, $zero, CTLP_OFFSET_EX_C1($c1)

    cld         $k0, $zero, (CONTEXT_OFFSET_CAUSE)($kr2c)

exception_return_main:

    # Check we were actually in exception level

    cld         $k1, $zero, (CONTEXT_OFFSET_EX_STATE)($kr2c)
    andi        $k1, $k1, EX_STATE_EL
    daddiu      $k1, $k1, -(EX_STATE_EL)
    bnez        $k1, bad_use_of_return
    cld         $k1, $zero, (CONTEXT_OFFSET_EX_STATE)($kr2c)
    daddiu      $k1, $k1, -(EX_STATE_EL)
    csd         $k1, $zero, (CONTEXT_OFFSET_EX_STATE)($kr2c)

    # Restore ccause
    cld         $k1, $zero, (CONTEXT_OFFSET_CCAUSE)($kr2c)

    # Swap pcc
    clcbi       $epcc, CTLP_OFFSET_EX_PCC($c1)
    clcbi       $idc, (CONTEXT_OFFSET_EX_PCC)($kr2c)          # idc used as tmp here
    cscbi       $idc, CTLP_OFFSET_EX_PCC($c1)

    # Swap idc
    clcbi       $idc, CTLP_OFFSET_EX_IDC($c1)
    clcbi       $kr2c, (CONTEXT_OFFSET_EX_IDC)($kr2c)
    cscbi       $kr2c, CTLP_OFFSET_EX_IDC($c1)

    # Load c1 - if this is a replay go on to do a normal exception
    bnez        $k0, context_switch_exception_entry_custom_ccause
    clcbi       $c1, CTLP_OFFSET_EX_C1($c1)

    # Otherwise eret
    ERET_BUGLESS

bad_use_of_return:

############################################################
# void, exception_signal(context_t other_context, register_t code)
.global exception_signal
exception_signal:
############################################################

    # We need 2 paths here. If the context is running, we trigger an exception.
    # If not, we just modify its saved context so next time it restores the exception will have happened
    # TODO
    CRETURN

exception_user_handle: # TODO our idc needs an appropriate entry for this

    # idc is USER idc - we look for exception pcc and idc
    # kr1c is the per physical cpu state (can't clobber)
    # kr2c is our context
    # k0 is cause
    # ccause already stored

    # Can't write to any user registers while in exception as a TLB abort will kill us.
    # We will restart the exception if we get a TLB fault, we should take the same exception

    # the first entry we need for the context already touched
    cld         $k1, $zero, (CONTEXT_OFFSET_EX_SAVED_IDC)($kr2c) # (cause TLB fault for context)
    clcbi       $kr2c, CTLP_OFFSET_EX_PCC($idc)          # kr2c = uex_pcc (also causes tlb fault)
    cbtu        $kr2c, context_switch_exception_entry    # if(untagged(uex_pcc))
    nop # By point we are definately going propagate to the user. But we need to take any TLB faults now.
    cscbi       $c1, CTLP_OFFSET_EX_C1($idc)             # saved_c1 = c1 (also causes TLB fault)

    # At this point we can no longer take a TLB fault. Safe to clobber user registers.
    # restore kr2c to our context
    clc     $kr2c, $zero, current_context($kr1c)

    # Store causes for when the user requests them.
    csd         $k0, $zero, (CONTEXT_OFFSET_CAUSE)($kr2c)
    cgetcause   $k0
    csd         $k0, $zero, (CONTEXT_OFFSET_CCAUSE)($kr2c)

    # Set User Exception Level - this is not re-entrant
    cld         $k0, $zero, (CONTEXT_OFFSET_EX_STATE)($kr2c)
    ori         $k0, $k0, EX_STATE_EL
    csd         $k0, $zero, (CONTEXT_OFFSET_EX_STATE)($kr2c)

    # Swap idc,pcc with uex. Save uex somewhere else for the next exception. Save eidc for restore help.
    # None of the below can take a TLB exception as we loaded all of them above.

    clcbi       $c1, CTLP_OFFSET_EX_PCC($idc)       # c1 = uex_pcc
    cscbi       $epcc, CTLP_OFFSET_EX_PCC($idc)     # saved_pcc = epcc
    cscbi       $c1, (CONTEXT_OFFSET_EX_PCC)($kr2c)    # uex_pcc_to_next = uex_pcc (for next ex)
    cmove       $epcc, $c1                          # epcc = uex_pcc

    clcbi       $c1, CTLP_OFFSET_EX_IDC($idc)       # c1 = uex_idc
    cscbi       $idc, CTLP_OFFSET_EX_IDC($idc)      # uex_idc = eidc
    cscbi       $c1, (CONTEXT_OFFSET_EX_IDC)($kr2c) # = uex_idc_to_next = uex_idc (for next ex)
    cscbi       $idc, (CONTEXT_OFFSET_EX_SAVED_IDC)($kr2c) # saved_idc = eidc (to use for restore)
    cmove       $idc, $c1                           # idc = uex_idc

    ERET_BUGLESS

exception_user_handle_abort:
    clcbi   $c1, CTLP_OFFSET_EX_C1($idc)
    b context_switch_exception_entry
    dmfc0   $k0, $13

######################################### (safe)
# type_res_bitfield_t* tres_get_ro_bitfield(void)
.global tres_get_ro_bitfield
tres_get_ro_bitfield:
#########################################

    dli         $t0, tres_bitfield
    cincoffset  $c3, $idc, $t0
    dli         $t0, TRES_BITFIELD_SIZE
    csetboundsexact  $c3, $c3, $t0
    dli         $t0, (Perm_Load)
    candperm    $c3, $c3, $t0
    CRETURN



# TODO we have way too many error returns. This one is pretty general, use it in lots of places
# TODO to make the nano kernel smaller
nano_er:
    cclearlo    EN11(c3,c4,c5,c6,c7,c8,c9,c10,c13,c14,c15)
    CRETURN

############################################
# Some stuff used by the exception vectors #
############################################

tlb_table_missing: # (safe) (k1 will contain bad entry. 0 = needs mapping. -1 = already used (supressable)
# Look for magic nop after this instruction. If it is present, we advance pcc and no exception
    cld         $k0, $zero, in_switch($kr1c)
    bnez        $k0, nano_kernel_die
    cgetepcc    $epcc
    cld         $k0, $zero, exception_level($kr1c)
    bnez        $k0, nano_kernel_die
    mfc0        $k0, $14                # TODO FPGA BUG
    csetoffset  $epcc, $epcc, $k0       # TODO FPGA BUG

    cld         $k0, $zero, unested_cause($kr1c)      # If we took an exception trying this last time - report FIRST exception
    bnez        $k0, context_switch_exception_entry
    csd         $zero, $zero, unested_cause($kr1c)
    dmfc0       $k0, MIPS_CP0_REG_CAUSE
    csd         $k0, $zero, unested_cause($kr1c)     # store our current cause. If this causes an exception don't try again
    li          $k0, 0x3400d00d
    clw         $k1, $zero, 4($epcc)
    beq         $k0, $k1, skip_and_return
    csd         $zero, $zero, unested_cause($kr1c)
    b           context_switch_exception_entry
    dmfc0       $k0, MIPS_CP0_REG_CAUSE


get_interface: # (safe)
    dla         $k0, locals_start
    dli         $k1, locals_size
    csetoffset  $kr2c, $kdc, $k0
    csetboundsexact  $kr2c, $kr2c, $k1                        # kr1c will hold a capability to our locals

    # TODO have a per context vector to say which functions are allowed
    dsll        $k1, $a1, CAP_SIZE_BITS

    dli         $k0, NANO_KERNEL_TYPE
    csetoffset  $c1, $kdc, $k0                           # c1 holds the sealing capability for our plt.got
    cseal       $c2, $kr2c, $c1                          # seal local into c2
    clc         $c1, $k1, create_context_cap($kr2c)      # load code cap into c1

skip_and_return:
    cincoffset  $epcc, $epcc, 4

    ERET_BUGLESS

shootdown_request:
    # Perform a shootdown then eret. We may take another exception immediately but this OK
    invalidate_tlb  $k0, $k1
    li          $k0, 1
    csd         $k0, $zero, since_shotdown($kr1c)

#ifdef HARDWARE_qemu
    dmfc0       $k0, MIPS_CP0_REG_CAUSE
    li          $k1, ~(0b11 << MIPS_CP0_STATUS_IM_SHIFT)
    and         $k0, $k0, $k1
    dmtc0       $k0, MIPS_CP0_REG_CAUSE
#else
    dli         $k0, (PHY_MEM_START_UNCACHED + PIC_CONFIG_START + (8 * 1024) + 256 + INTERRUPTS_NANO_PIC_SHOOTDOWN_OFF)
    get_cpu_id  $k1
    dsll        $k1, $k1, PIC_CONFIG_SZ_LOG_2
    daddu       $k0, $k0, $k1
    dli         $k1, INTERRUPTS_NANO_PIC_SHOOTDOWN_BIT
    csb         $k1, $k0, 0($kdc)
#endif
    ERET_LEGACY

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
# TODO if an exception happens in something like context switch we should die more nicely

        # Temporary hack coz hardware is wrong
        dmfc0       $k0, $14            # TODO FPGA BUG
        cgetepcc    $epcc
        csetoffset  $epcc, $epcc, $k0   # TODO FPGA BUG

        # End hack
        dmfc0       $k0, MIPS_CP0_REG_CAUSE
        andi        $k1, $k0, (INTERRUPTS_NANO_OWNED << MIPS_CP0_STATUS_IM_SHIFT)
        bnez        $k1, shootdown_request
        andi        $k0, $k0, 0xFF        # k0 has cause
        bnez        $a0, 1f
        daddiu      $k1, $k0, -(MIPS_CP0_EXCODE_SYSCALL << MIPS_CP0_CAUSE_EXCODE_SHIFT) # syscall with a0 = 0 results in getting nano kernel IF
        beqz        $k1, get_interface
1:      # If this is a TLB error, first go to the TLB handle. We must allow TLB faults in exception levels etc.1:
        daddiu      $k1, $k0, -(MIPS_CP0_EXCODE_TLBL << MIPS_CP0_CAUSE_EXCODE_SHIFT)
        beqz        $k1, tlb_trampoline
        daddiu      $k1, $k0, -(MIPS_CP0_EXCODE_TLBS << MIPS_CP0_CAUSE_EXCODE_SHIFT)
        beqz        $k1, tlb_trampoline

# If k0 is zero (this is a HW interrupt) We can supress with ex level. o/w it is a software exception.
# These can be user handled or skipped with magic
        cld         $k1, $zero, in_switch($kr1c)
        bnez        $k1, nano_kernel_die
        nop
        beqz        $k0, hw_ex
soft_ex:

        # We supress exception if a magic sequence is present. This is to allow default behavior with bad caps
        cld         $k0, $zero, unested_cause($kr1c)      # If we took an exception trying this last time - report FIRST exception
        bnez        $k0, 2f
        csd         $zero, $zero, unested_cause($kr1c)
        dmfc0       $k0, MIPS_CP0_REG_CAUSE
        csd         $k0, $zero, unested_cause($kr1c)     # store our current cause. If this causes an exception don't try again
        li          $k0, 0x3400d00d
        clw         $k1, $zero, 4($epcc)
        beq         $k0, $k1, skip_and_return
        csd         $zero, $zero, unested_cause($kr1c)
        dmfc0       $k0, MIPS_CP0_REG_CAUSE
2:
        # We can also pass to the user
        clc     $kr2c, $zero, current_context($kr1c)
        cbtu    $idc, context_switch_exception_entry # No IDC = no user interrupts
        cld     $k1, $zero, (CONTEXT_OFFSET_EX_STATE)($kr2c)
        daddiu  $k1, $k1, -EX_STATE_UH
        bnez    $k1, context_switch_exception_entry # Must have user bit set, and not already be in an exception
        nop
		b exception_user_handle # All of the above pass - user can take his own exception
		nop

hw_ex:
        cld         $k1, $zero, exception_level($kr1c)
        beqz        $k1, context_switch_exception_entry
        dmfc0       $k0, MIPS_CP0_REG_CAUSE
        dmfc0       $k1, MIPS_CP0_REG_STATUS
        li          $k0, 1
        csd         $k0, $zero, supressed($kr1c)
        not         $k0, $k0
        and         $k1, $k1, $k0 # As interrupts had to be enabled
        mtc0        $k1, $12      # Disable interrupts, we are happy to take the hit as this is rare
        ERET_LEGACY

kernel_exception_trampoline_end:
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
# dont use kr2c unless context switch restores it
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

    csd         $zero, $zero, unested_cause($kr1c)                 # if this was a nested exception we are restarting now
    # Setup virtual page mask
    # dli         $k0, (1 << (PHY_PAGE_SIZE_BITS - 12)) - 1
    # TODO check the top bits of the vaddr

    dmtc0       $k1, cp0_entrylo0
    # dmtc0       $k0, cp0_pagemask
    daddiu      $k1, $k1, (1 << PFN_SHIFT)                          # make second maping the next page
    dmtc0       $k1, cp0_entrylo1

    cld         $k1, $zero, in_switch($kr1c)
    bnez        $k1, 1f                                             # if we were switch epcc is a restart point
    tlbwr                                                           # write tlb entry
    # We are here from either...
    #   a) A normal TLB miss, just eret
    #   b) A TLB exception from one of our 'checkpoints'
    eret


1:  cjr         $epcc
    nop
tlb_trampoline_end:
    .global tlb_trampoline_end
    .end tlb_trampoline
    .size tlb_trampoline, tlb_trampoline_end - tlb_trampoline

nano_kernel_end:
.size nano_kernel, nano_kernel_end - nano_kernel_start