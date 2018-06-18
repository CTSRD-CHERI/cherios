/*-
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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

#ifndef CHERIOS_CHERIPLT_H
#define CHERIOS_CHERIPLT_H

// While we do not have linker support, this header is used to create a PLT like construct, but every entry in the
// .plt.got is a SEALED pointer.

// PLT_WTYPE will define a struct representing the plt, it is up to the user to create one of these
// It will also create a set of ccall methods for each member of the struct.
// It will also create a set of default objects for each member
// Again, it is up to the user to populate these default objects

// A helpful init method is also provided. It will populate the default objects from the struct, with a default data

// The stubs are now as they will eventually be in the linker

#if _MIPS_SZCAP == 256
        #define PLT_STUB PLT_STUB256
        #define PLT_STUB_CSD PLT_STUB256_CSD
        #define PLT_STUB_DATA_OFF 32
#elif _MIPS_SZCAP == 128
    #define PLT_STUB PLT_STUB128
    #define PLT_STUB_CSD PLT_STUB128_CSD
    #define PLT_STUB_DATA_OFF 16
#else
    #error Unknown capability size
#endif

#ifndef __ASSEMBLY__

#include "cheric.h"
#include "ccall.h"
#include "utils.h"

// FIXME: alias needs size too
#define PLT_STUB256(name, obj, tls, tls_reg, alias)  \
__asm__ (                       \
    SANE_ASM                    \
    ".text\n"                   \
    ".p2align 5\n"              \
    ".global " #name "\n"       \
    ".ent " #name "\n"          \
    "" #name ":\n"              \
    alias                       \
    "clc         $c1, $zero, (1*32)($c12)  \n"        \
    "clc         $c12, $zero, (2*32)($c12)\n"         \
    "cjr         $c12                                 \n"   \
    "clcbi       $c2, %captab" tls "20(" EVAL5(STRINGIFY(obj)) ")(" tls_reg ")\n"     \
    ".space (1 * 16) \n"        \
    ".global " #name "_data\n"  \
    "" #name "_data:\n"         \
    ".space (2 * 32) \n"        \
    ".size " #name "_data, 64\n"\
    ".end " #name "\n"          \
);

// This is an inlined mode for single domain, can use this if we know statically our trust relationship
#define PLT_STUB256_CSD(name, obj, tls, tls_reg, alias) \
__asm__ (                       \
    SANE_ASM                    \
    ".text\n"                   \
    ".p2align 5\n"              \
    ".global " #name "\n"       \
    ".ent " #name "\n"          \
    "" #name ":\n"              \
    alias                       \
    "clc         $c1, $zero, (1*32)($c12)  \n"        \
    "clcbi       $c2, %captab" tls "20(" EVAL5(STRINGIFY(obj)) ")(" tls_reg ")\n"     \
    "ccall       $c1, $c2, 2 \n"\
    "nop\n"                     \
    ".space (1 * 16) \n"        \
    ".global " #name "_data\n"  \
    "" #name "_data:\n"         \
    ".space (2 * 32) \n"        \
    ".size " #name "_data, 64\n"\
    ".end " #name "\n"          \
);

#define PLT_STUB128(name, obj, tls, tls_reg, alias) \
__asm__ (                       \
    SANE_ASM                    \
    ".text\n"                   \
    ".p2align 4\n"              \
    ".global " #name "\n"       \
    ".ent " #name "\n"          \
    "" #name ":\n"              \
    alias                       \
    "clc         $c1, $zero, (1*16)($c12)  \n"        \
    "clc         $c12, $zero, (2*16)($c12)\n"         \
    "cjr         $c12                                 \n"   \
    "clcbi       $c2, %captab" tls "20(" EVAL5(STRINGIFY(obj)) ")(" tls_reg ")\n"     \
    ".global " #name "_data\n"  \
    "" #name "_data:\n"         \
    ".space (2 * 16) \n"        \
    ".size " #name "_data, 32\n"\
    ".end " #name "\n"          \
);

#define PLT_STUB128_CSD(name, obj, tls, tls_reg, alias) \
__asm__ (                       \
    SANE_ASM                    \
    ".text\n"                   \
    ".p2align 4\n"              \
    ".global " #name "\n"       \
    ".ent " #name "\n"          \
    "" #name ":\n"              \
    alias                       \
    "clc         $c1, $zero, (1*16)($c12)  \n"        \
    "clcbi       $c2, %captab" tls "20(" EVAL5(STRINGIFY(obj)) ")(" tls_reg ")\n"     \
    "ccall       $c1, $c2, 2 \n"\
    "nop\n"                     \
    ".global " #name "_data\n"  \
    "" #name "_data:\n"         \
    ".space (2 * 16) \n"        \
    ".size " #name "_data, 32\n"\
    ".end " #name "\n"          \
);

typedef void common_t(void);

struct pltstub256 {
    capability c1;
    common_t* mode;
};



    #define PLT_GOT_ENTRY(name, ...) capability name;

    #define STUB_STRUCT(name, auth) ((struct pltstub256*)(rederive_perms((((char*)&name) + PLT_STUB_DATA_OFF), auth)))
    #define STUB_STRUCT_RO(name) ((struct pltstub256*)((((char*)&name) + PLT_STUB_DATA_OFF)))
    #define PLT_UNIQUE_OBJECT(name) name ## _data_obj

    #define DECLARE_STUB(name, ret, sig, ...) extern ret name sig; extern struct pltstub256 name ## _data;

    #define GET_ALIAS(X, ...) X
    #define DEFINE_STUB(name, ret, sig, type, ST, tls, tls_reg, ...) ST(name, PLT_UNIQUE_OBJECT(type), tls, tls_reg, GET_ALIAS(__VA_ARGS__,))

    #define DECLARE_DEFAULT(type, per_thr) extern per_thr capability PLT_UNIQUE_OBJECT(type);
    #define ALLOCATE_DEFAULT(type, per_thr) per_thr capability PLT_UNIQUE_OBJECT(type);

    #define INIT_OBJ(name, ret, sig, mode, auth, ...)             \
        {struct pltstub256* ob = STUB_STRUCT(name, auth);         \
        ob->c1 = plt_if -> name;                            \
        ob->mode = mode;}

    #define DECLARE_PLT_INIT(type, LIST, tls_reg, tls)                                 \
    static inline void init_ ## type (type* plt_if, capability data, common_t* mode, capability auth) {      \
        __asm__ ("cscbi %[d], %%captab" tls "20(" #type "_data_obj)(" tls_reg ")\n"::[d]"C"(data):); \
        LIST(INIT_OBJ, mode, auth)                                                                \
    }\
    static inline void init_ ## type ##_new_thread(type* plt_if, capability data, common_t* mode, capability auth) {      \
            __asm__ ("cscbi %[d], %%captab" tls "20(" #type "_data_obj)(" tls_reg ")\n"::[d]"C"(data):); \
    }

    #define PLT_common(type, LIST, per_thr, tls_reg, tls)    \
    typedef struct                          \
    {                                       \
        LIST(PLT_GOT_ENTRY,)                \
    } type;                                 \
    DECLARE_DEFAULT(type, per_thr)          \
    LIST(DECLARE_STUB,)                     \
    DECLARE_PLT_INIT(type, LIST, tls_reg, tls)

    #define PLT(type, LIST) PLT_common(type, LIST,, "$c25",)
    #define PLT_thr(type, LIST) PLT_common(type, LIST,__thread,"$c26", "_tls")

    #define PLT_ALLOCATE_common(type, LIST, thread_loc, tls, tls_reg, ST) ALLOCATE_DEFAULT(type, thread_loc) LIST(DEFINE_STUB, type, ST, tls, tls_reg)


    #define PLT_ALLOCATE_csd(type, LIST)  PLT_ALLOCATE_common(type, LIST,,,"$c25",PLT_STUB_CSD)
    #define PLT_ALLOCATE(type, LIST) PLT_ALLOCATE_common(type, LIST,,,"$c25",PLT_STUB)
    #define PLT_ALLOCATE_tls(type, LIST) PLT_ALLOCATE_common(type, LIST,__thread,"_tls","$c26",PLT_STUB)

    // These are the mode stubs
    extern void plt_common_single_domain(void);
    extern void plt_common_complete_trusting(void);
    extern void plt_common_trusting(void);
    extern void plt_common_untrusting(void);

    // This is the fully untrusting entry stub
    extern void entry_stub(void);
#define MAKE_

#else // __ASSEMBLY__

    // If this is included from assembly, we should instead create a set of .set directives that define the offsets
    // In the .plt.got
    .set enum_ctr, 0
    #define SET_NAME(NAME, ...) .set NAME ## _offset, (enum_ctr * CAP_SIZE); .set enum_ctr, enum_ctr + 1;
    #define PLT(type, LIST) LIST(SET_NAME,)
    #define PLT_thr(type, LIST, ...) PLT(type, LIST)

#endif // __ASSEMBLY__

#endif //CHERIOS_CHERIPLT_H
