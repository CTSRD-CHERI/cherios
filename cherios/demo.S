# Demo assembly for the new ABI


# V2 #


#----   is  $cds (domain sealing capability)                        caller saved, can get from cgp
#$c11   is  $csp (stack pointer capability)                         callee saved
#----   is  $cusp (unsafe stack pointer capability)                 argument and callee return register
#----   is  $cbp (base pointer capability)                          callee saved

#$idc   is  $cgp (global pointer capability) / sealed invocation    caller saved, but restored via crd for free
#$c17   is  $cra (return address capability)                        callee saved
#$c18   is  $crd (return data capability)                           callee saved



#----   is  $cnsp (next unsafe stack pointer)                       variable or a register ?

# When a new unsafe allocation block is needed, we move up the linked list.
# when a function exits, we deallocate its blocks (max 2, one static, one dynamic) it moves down.

# We allocate safe space in $csp. We allocate unsafe space $cusp.
# When it comes to freeing unsafe space, we just bump the start of the buffer and return its new value via $cusp
# Unsafe allocations are always at the start of a buffer, maybe followed by safe allocations

# Layout of globals table:

###########
# Guard   # <-- $cgp
# csp     #
# cusp    #
# rtramp  #
# cds     #
# prev    #       this is a pointer to previous outstanding return struct
###########
# GLobals #
###########










# An unsafe stack (not in use):

######### <--- cusp
# cnsp  #
#########

# An unsafe stack (in use):

##################
# UNSAFE dynamic # <-- cusp (cnsp stored elsewhere)
##################

# A safe stack:

#################
# UNSAFE static #
# SAFE static   #
################# <-- cbp
# SAFE dynamic  #
################# <-- csp



# A return closure :

#########
# Guard # <- idc
# csp   #
#########



#enum  stack_state{
#    callable_ready = 1,
#    callable_taken = 0,

#    returnable_ready = 3,
#    returnable_blocked = 4,
#    returnable_used = 5
#}

#define AD_CNSP_OFF
#define AD_SCNSP_OFF


# I have inlined nicely calls to these in the stubs below, but alloca would result in these

.macro ALLOCATE_SAFE_DYNAMIC x, reg
    cincoffset $csp, $csp, -\x
    csetbounds \reg, $csp, \x
.endm

.macro ALLOCATE_UNSAFE_DYNAMIC x
    cincoffset $cusp, $cusp, -\x
    csetbounds \reg, $cusp, \x
.enm

# This is used at the start of functions

.macro ALLOCATE_UNSAFE_BLOCKS

    .if USES_STATIC_UNSAFE                      # we need a block for unsafe static storage. Bump here.
        csc   $csp, $zero, -(UNSAFE+CAP_SIZE)($cusp)    # the first safe variable stored is callers csp
        cmove $csp, $cusp
        clc   $cusp, $zero, (-CAP_SIZE)($csp)
    .endif

    .if USES_DYNAMIC_UNSAFE                     # we need a block for unsafe dynamic storage. Save next for calls our #
        clc  $cnsp, $zero, (-CAP_SIZE)($cusp)
    .endif

.endm

# this on return

.macro FREE_UNSAFE_BLOCKS
    .if USES_DYNAMIC_UNSAFE
        csetboundsback  $cusp, $cusp, $zero
        csc             $cnsp, $zero, -CAP_SIZE($cusp)      # deallocate unsafe dynamic
    .endif

    .if USES_STATIC_UNSAFE
        csetboundsback  $cusp, $cbp, SAFE           # reclaim static space and dynamic_safe
        # and csp?
        RESTORE_REGS($cusp)                         # will also restore things like csp
    .else
        cincoffset      $csp, $cbp, SAFE            # reclaim static_safe and dynamic_safe
        RESTORE_REGS($cbp)
    .endif

.endm

.macro call target

    SAVE($cbp)

    .if USES_DYNAMIC_UNSAFE
        csc         $csp, $zero, (-CAP_SIZE)($cusp)
        cmove       $csp, $cusp
        cmove       $cusp, $cnsp
    .endif

    cjalr       \target, $cra
    cmove       $crd, $cgp

    after:                                              # after call idc will have been restored by ccall, and
                                                        # the stack by callee / or the plt stub

    .if USES_DYNAMIC_UNSAFE
        cmove       $cnsp, $cusp
        cmove       $cusp, $csp
        clc         $csp, $zero, (-CAP_SIZE)($cusp)
    .endif

    RESTORE($cbp)

.endm


################
# A function : #
################

cross_domain_prolog: #<-- expose this normally for cross domain calls
1: clld     $at, $cgp
tnei        $at, callable_ready
cscd        $at, $zero, $cgp
beqz        $at, 1b
nop

complete_trust_cross_domain: #<-- expose this if we trust everyone we link with (trusting-total)

cmove       $ctmp, $csp
clc         $csp, $zero, (CAP_SIZE)($cgp)

csc         $cra, $zero, 0($csp)                # save caller state for epilog trampoline
csc         $crd, $zero, CAP_SIZE($csp)
csc         $ctmp, $zero, (2*CAP_SIZE)($csp)

clc         $cusp, $zero, (2*CAP_SIZE)($cgp)
clc         $cra,  $zero, (3*CAP_SIZE)($cgp)    # set return to our return trampoline
cmove       $crd,  $cgp

prolog:  # <-- jump locally to this

# At this point, csp, cusp, cgp, cra, and crd are all sensible, either by the caller or the prolog

ALLOCATE_UNSAFE_BLOCKS                      # between 0 and 4 instructions
cincoffset  $csp, $csp, -(SAFE + UNSAFE)    # allocates space for static stack
SAVE($csp)
cmove       $cbp, $csp

# ... code ...

epilog:
FREE_UNSAFE_BLOCKS                          # between 1 and 3 + restores
ccall       $cra, $crd, 2
# <-- pull last instruction into delay slot here






###########################################
# The return trampoline (one per system?) #
###########################################

epilog_domain_cross:                                # call will trampoline here if returning across a domain

csc         $cusp, $zero, (2*CAP_SIZE)($cgp)        # cusp may have been updated

clc         $cra, $zero, 0($csp)                    # load back callers state
clc         $crd, $zero, CAP_SIZE($csp)             #
clc         $csp, $zero, (2*CAP_SIZE)($csp)         #


# Trusted callers expect us to help them
cgetsealed  $at, $crd
bnez        2f
nop

clc         $cusp, $zero, (2*CAP_SIZE)($crd)
1: clld     $at, $crd
tnei        $at, callable_ready
cscd        $at, $zero, $crd
beqz        $at, 1b


2: # Dont need these three instructions in trust complete mode - they set our guard ready to recieve a call
dli         $at, callable_ready
sync
csd         $at, $zero, 0($cgp)


ccall       $cra, $crd, 2
nop





################################################
# Cross domain call (trusting-total) PLT STUB  #
################################################


get_code_ptr    $c1
get_data_ptr    $c2
cincoffset      $ctmp, $csp, ???
csc             $ctmp, $zero, CAP_SIZE($cgp)
ccall           $c1, $c2, 2
csc             $cusp, $zero, (2*CAP_SIZE)($cgp)


##################################################################################
# Cross domain call (trusting, but there exists an untrusted library) - PLT STUB #
##################################################################################

get_code_ptr    $c1
get_data_ptr    $c2
cincoffset      $ctmp, $csp, ???
csc             $ctmp, $zero, CAP_SIZE($cgp)
csc             $cusp, $zero, (2*CAP_SIZE)($cgp)
dli             $at, callable_ready
sync
ccall           $c1, $c2, 2
# set our guard on the way out. Our (trusted) caller will set it back for us
csd             $at, $zero, 0($cgp)


#################################################
# Cross domain call (untrusting) - via PLT stub #
#################################################

# TODO : .......

get_code_ptr    $c1
get_data_ptr    $c2

# ^^^ do those then jump to common?


# Untrsting call common
SAVE_CALLEE_SAVED($csp) # We need to save these as well, also allocate us 2 caps worth of space



# we need some static unsafe, so do the bumping thing and save that for return calls
# our sealed data

clc             $cds, $zero, (2*CAP_SIZE)($cgp)  # we need cds to construct return

cincoffset      $ctmp, $csp, ???
csc             $ctmp, $zero, CAP_SIZE($cgp)
csc             $cusp, $zero, (2*CAP_SIZE)($cgp)        # normal stores

csc             returnable_ready, $zero, (-CAP_SIZE)($csp) # create guard

cseal           $crd, $csp, $cds
cgetpcc         $cra
cincoffset      $cra, $cra, ???
cseal           $cra, $cra, $cds
cclearhi
ccall           $cra, $crd, 2
cclearlo

# Untrusting return common

# idc will be a struct containing all the data we need (guard,cgp,csp,cusp,prev