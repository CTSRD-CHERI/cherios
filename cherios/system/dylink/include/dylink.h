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
#include "nonce.h"

#define VIS_EXTERNAL __attribute__((visibility("default")))
#define VIS_HIDDEN __attribute__((visibility("hidden")))


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

#define DYLINK_GET_REQUIREMENTS_PORT_NO 0
#define DYLINK_NEW_PROCESS_PORT_NO      1

// To avoid having to do any dynamic allocation yet
#define MAX_LIBS_BASE_16 0, 0, 2, 0
#define MAX_LIBS 0x20

#define PLT_STUB_CODE_SIZE (4*4)
#define PLT_STUB_SIZE (4*4 + sizeof(capability)) // 4 instructions + a cap

#define MODE_NDX_OFF 6
#define DATA_NDX_OFF 14

#ifndef __ASSEMBLY__

#include "stddef.h"
#include "elf.h"

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

#define CLCBI_IM_OFFSET 2
#define CLCBI_IM_SCALE  4

#define get_sym_captable_offset32(Sym) (uint32_t)({                     \
uint32_t __ret;                                                           \
__asm__ ("lui %[ret], %%captab_hi(" X_STRINGIFY(Sym) ")\n"              \
         "daddiu %[ret], %[ret], %%captab_lo(" X_STRINGIFY(Sym) ")\n"   \
: [ret]"=r"(__ret) ::);     \
__ret;})

#define get_sym_call_captable_offset32(Sym) (uint32_t)({                \
uint32_t __ret;                                                         \
__asm__ ("lui %[ret], %%capcall_hi(" X_STRINGIFY(Sym) ")\n"              \
         "daddiu %[ret], %[ret], %%capcall_lo(" X_STRINGIFY(Sym) ")\n"   \
: [ret]"=r"(__ret) ::);     \
__ret;})

// There is currently no TLS captab_hi/lo so we are forced to extract the bits from a clcbi

#define get_tls_sym_captable_ndx16(Sym) (uint16_t)({                \
uint16_t __ret;                                                     \
__asm__ (".weak " X_STRINGIFY(Sym) "\n"                             \
         ".hidden "  X_STRINGIFY(Sym) "\n"                          \
         "clcbi $c1, %%captab_tls20(" X_STRINGIFY(Sym) ")($c26)\n"  \
         "cgetpcc $c1 \n"                                           \
         "clh %[ret], $zero, -2($c1) \n"                            \
: [ret]"=r"(__ret) ::"$c1");     \
__ret;})

// Actually has a type of whatever function pointer is passed in invoke_c1
extern void call_function_pointer_arg_mem(void);

// FIXME: have to pass ptr/ptr_data as c1/c2
// FIXME: have to cclearreg depends on number of varargs
#define INVOKE_FUNCTION_POINTER(ptr, ptr_data, ...) ({ \
    SET_TLS_SYM(invoke_c1,ptr);\
    SET_TLS_SYM(invoke_c2,ptr_data);\
    ((typeof(ptr))(&call_function_pointer_arg_mem))(__VA_ARGS__);})

#define SYM_NOT_FOUND ((capability)(-1))

typedef struct parsed_dynamic {
    unsigned const char* needed[MAX_LIBS];
    unsigned const char* strtab;
    const Elf64_Sym* symtab;
    const Elf64_Rel* jmprel;
    const Elf64_Rel* rel;
    const Elf64_Hash* hash;

    size_t jmprel_ents;
    size_t rel_ents;
    size_t needed_ents;
    size_t symtab_ents;

} parsed_dynamic_t;

typedef struct lib_info lib_info_t;

typedef struct link_partner {
    act_kt server_act;
    found_id_t* id;
    lib_info_t* info;
    size_t ndx;
} link_partner_t;

typedef struct link_session {
    link_partner_t partners[MAX_LIBS+1];
    size_t n_libs;
} link_session_t;

// To cut an annoying knot
typedef capability mop_t;

typedef union {
    cert_t as_signed;
    lib_info_t* as_unsigned;
} lib_unchecked_info_t;

typedef struct new_thread_request {
    res_t locals_res;
    res_t stack_res;
    res_t ustack_res;
} new_thread_request_t;

typedef struct new_process_request {
    nonce_t nonce;
    found_id_t* client_id;
    res_t globals_res;
    res_t session_res;
    new_thread_request_t first_thread;
} new_process_request_t;

struct lib_info {
    // Only needed when starting a new process
    size_t space_for_globals;
    size_t space_for_session;
    size_t space_for_plt_stubs;
    // Needed when starting a new process or thread
    size_t space_for_locals;
    // These functions are used for symbol linking after a new process is created with the server
    void (*provide_common)(act_control_kt self_ctrl, act_kt self_ref, mop_t mop);
    void (*provide_session)(link_session_t* session, res_t plt_res, int first_thread, capability* data_args);
    void (*resolve_syms)(const unsigned char** in, capability* out, size_t n_syms, int functions);
    // This last one is server only, and can be called BEFORE a new thread is created
    capability (*new_library_thread)(new_thread_request_t* thread_request);
};

extern lib_info_t own_info;
extern parsed_dynamic_t own_pd;

// Parses dynamic section
void parse_dynamic_section(parsed_dynamic_t* pd);

// Executables calls this to dylink
void auto_dylink_new_process(link_session_t* session, capability* data_args);
void auto_dylink_pre_new_thread(link_session_t* session, capability* data_args);
void auto_dylink_post_new_thread(link_session_t* session, capability* data_args);

// Link-interface functions
void set_info_functions(lib_info_t* info);

#endif // ASSEMBLY

#define CROSS_DOMAIN(X) (__cross_domain_ ## X)
#define TRUSTED_CROSS_DOMAIN(X) (__cross_domain_trusted_ ## X)

#define SEALED_CROSS_DOMAIN(X) cheri_seal(&CROSS_DOMAIN(X), get_ctl()->cds)

#define TRUSTED_DATA get_ctl()
#define UNTRUSTED_DATA ({CTL_t* _ctl_tmp = get_ctl(); cheri_seal(_ctl_tmp,_ctl_tmp->cds);})

// These allow you to pick an overridable default
#if(FORCE_SECURE)
    #define DATA_DEFAULT_SECURE     UNTRUSTED_DATA
    #define DATA_DEFAULT_INSECURE   UNTRUSTED_DATA

    #define CROSS_DOMAIN_DEFAULT_SECURE(X) CROSS_DOMAIN(X)
    #define CROSS_DOMAIN_DEFAULT_INSECURE(X) CROSS_DOMAIN(X)
    #define CROSS_DOMAIN_DEFAULT_SECURE_SEALED(X)   SEALED_CROSS_DOMAIN(X)
    #define CROSS_DOMAIN_DEFAULT_INSECURE_SEALED(X) SEALED_CROSS_DOMAIN(X)

#elif(FORCE_INSECURE)
    #define DATA_DEFAULT_SECURE     TRUSTED_DATA
    #define DATA_DEFAULT_INSECURE   TRUSTED_DATA

    #define CROSS_DOMAIN_DEFAULT_SECURE(X) TRUSTED_CROSS_DOMAIN(X)
    #define CROSS_DOMAIN_DEFAULT_INSECURE(X) TRUSTED_CROSS_DOMAIN(X)
    #define CROSS_DOMAIN_DEFAULT_SECURE_SEALED(X)   TRUSTED_CROSS_DOMAIN(X)
    #define CROSS_DOMAIN_DEFAULT_INSECURE_SEALED(X) TRUSTED_CROSS_DOMAIN(X)

#else
    #define DATA_DEFAULT_SECURE     UNTRUSTED_DATA
    #define DATA_DEFAULT_INSECURE   TRUSTED_DATA

    #define CROSS_DOMAIN_DEFAULT_SECURE(X) CROSS_DOMAIN(X)
    #define CROSS_DOMAIN_DEFAULT_INSECURE(X) TRUSTED_CROSS_DOMAIN(X)
    #define CROSS_DOMAIN_DEFAULT_SECURE_SEALED(X)   SEALED_CROSS_DOMAIN(X)
    #define CROSS_DOMAIN_DEFAULT_INSECURE_SEALED(X) TRUSTED_CROSS_DOMAIN(X)
#endif

#endif //CHERIOS_DYLINK_H
