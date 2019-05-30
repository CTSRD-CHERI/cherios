/*-
 * Copyright (c) 2017 Lawrence Esswood
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
#ifndef CHERIOS_DYLINK_H
#define CHERIOS_DYLINK_H

#include "string_enums.h"
#include "cheric.h"

#define DOMAIN_TYPE_LIST(ITEM)  \
    ITEM(callable_taken, 0)     \
    ITEM(callable_ready, 1)     \
    ITEM(returnable_ready, 2)   \
    ITEM(returnable_used, 3)    \
    ITEM(reserved_start, 4)     \
    ITEM(reserved_end, 254)     \
    ITEM(user_type, 255)

DECLARE_ENUM(domain_type_t, DOMAIN_TYPE_LIST)

#define DEFINE_DOMAIN_T     DEFINE_ENUM_CASE(domain_type_t, DOMAIN_TYPE_LIST)

#define DOMAIN_TYPE_MASK    0xff
#define DOMAIN_INFO_SHIFT   8

// These offsets are required by the nano-kernel. An exception will SWAP pcc/idc and STORE c1
// An exception restore call will RESTORE pcc/idc/c1, and write the previous values of pcc and idc
#define CTLP_OFFSET_EX_PCC  (CAP_SIZE)
#define CTLP_OFFSET_EX_IDC  (CAP_SIZE * 2)
#define CTLP_OFFSET_EX_C1   (CAP_SIZE * 3)

// These are our own
#define CTLP_OFFSET_CSP  (CAP_SIZE*4)
#define CTLP_OFFSET_CUSP (CAP_SIZE*5)
#define CTLP_OFFSET_CDS  (CAP_SIZE*6)
#define CTLP_OFFSET_CDL  (CAP_SIZE*7)
#define CTLP_OFFSET_CGP  (CAP_SIZE*8)

// From unsafe stack base
#define CSP_OFF_NEXT    (-1 * CAP_SIZE)
// From unsafe stack end
#define CSP_OFF_PREV    (-2 * CAP_SIZE)

#define STACK_LINK_SIZE (2 * CAP_SIZE)

#ifndef __ASSEMBLY__

#include "stddef.h"

#define MAKE_USER_GUARD_TYPE(info) (((info) << DOMAIN_INFO_SHIFT) | user_type)
#define GET_GUARD_TYPE(guard) ((domain_type_t)((guard) & DOMAIN_TYPE_MASK))
#define GET_GUARD_INFO(guard) ((guard) >> DOMAIN_INFO_SHIFT)

typedef struct guard_t {
    register_t guard; // of type (domain_type_t | (info << 8))
    char pad[CAP_SIZE - sizeof(register_t)];
} guard_t;

typedef void entry_stub_t(void);

typedef void ex_pcc_t(void);

typedef struct CTL_t {
    guard_t guard;
    // On exception pcc/idc will do set to these.
    // All 3 will have the exceptional pcc/idc/c1 saved in them
    // On restore all 3 will be restored, and ex_pcc/_exidc will be what they originally were
    // BEFORE the exception (i.e. the handler is unchanged)
    // Note c1 is a SCRATCH register on entry to your exception routine
    // WARN If the other of these is changed TLB faulting code in the nano kernel need changing
    ex_pcc_t* ex_pcc;
    capability ex_idc;
    capability ex_c1;
    capability csp;
    capability cusp;
    sealing_cap cds;
    entry_stub_t* cdl;
    capability cgp;
    capability captable[];
} CTL_t;

_Static_assert(offsetof(CTL_t, guard) == 0,             "CGP offsets need to match assembly assumptions");
_Static_assert(offsetof(CTL_t, ex_pcc) == CTLP_OFFSET_EX_PCC,  "CGP offsets need to match assembly assumptions");
_Static_assert(offsetof(CTL_t, ex_idc) == CTLP_OFFSET_EX_IDC,  "CGP offsets need to match assembly assumptions");
_Static_assert(offsetof(CTL_t, ex_c1) == CTLP_OFFSET_EX_C1,  "CGP offsets need to match assembly assumptions");
_Static_assert(offsetof(CTL_t, csp) == CTLP_OFFSET_CSP,  "CGP offsets need to match assembly assumptions");
_Static_assert(offsetof(CTL_t, cusp) == CTLP_OFFSET_CUSP, "CGP offsets need to match assembly assumptions");
_Static_assert(offsetof(CTL_t, cds) == CTLP_OFFSET_CDS,  "CGP offsets need to match assembly assumptions");
_Static_assert(offsetof(CTL_t, cdl) == CTLP_OFFSET_CDL,  "CGP offsets need to match assembly assumptions");
_Static_assert(offsetof(CTL_t, cgp) == CTLP_OFFSET_CGP,  "CGP offsets need to match assembly assumptions");

#define get_ctl() ((CTL_t*)({                           \
CTL_t* __ret;                                             \
__asm__ ("cmove %[ret], $idc" : [ret]"=C"(__ret) ::);     \
__ret;}))

#define get_cds() (sealing_cap) ({\
sealing_cap __ret;                                             \
__asm__ ("clcbi %[ret], (%[im])($idc)" : [ret]"=C"(__ret) : [im]"i"(CTLP_OFFSET_CDS):);     \
__ret;})

static inline size_t ctl_get_num_table_entries(CTL_t* ctl) {
    return (cheri_getlen(ctl) / sizeof(capability)) - 9;
}

#define get_cgp() ((capability*)({                           \
capability* __ret;                                             \
__asm__ ("cmove %[ret], $c25" : [ret]"=C"(__ret) ::);     \
__ret;}))

// Actually has a type of whatever function pointer is passed in invoke_c1
extern void call_function_pointer_arg_mem(void);

// FIXME: have to pass ptr/ptr_data as c1/c2
// FIXME: have to cclearreg depends on number of varargs
#define INVOKE_FUNCTION_POINTER(ptr, ptr_data, ...) ({ \
    SET_TLS_SYM(invoke_c1,ptr);\
    SET_TLS_SYM(invoke_c2,ptr_data);\
    ((typeof(ptr))(&call_function_pointer_arg_mem))(__VA_ARGS__);})

#endif // ASSEMBLY

#define CROSS_DOMAIN(X) (__cross_domain_ ## X)
#define TRUSTED_CROSS_DOMAIN(X) (__cross_domain_trusted_ ## X)

#define SEALED_CROSS_DOMAIN(X) cheri_seal(&CROSS_DOMAIN(X), get_ctl()->cds)

#define TRUSTED_DATA get_ctl()
#define UNTRUSTED_DATA ({CTL_t* _ctl_tmp = get_ctl(); cheri_seal(_ctl_tmp,_ctl_tmp->cds);})

#endif //CHERIOS_DYLINK_H
