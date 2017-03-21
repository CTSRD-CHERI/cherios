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

#ifndef __ASSEMBLY__

#include "cheric.h"
#include "ccall.h"

    //TODO once we have proper linker support, we won't have to manually specify the interface like this
    #define PLT_UNIQUE_OBJECT(name) name ## _ ## default_obj

    #define PLT_GOT_ENTRY(name, ...) capability name;
    #define CCALL_WRAP(name, ret, sig, ...)                                                     \
                        __attribute__((cheri_ccall))                                            \
                        __attribute__((cheri_method_suffix("_inst")))                           \
                        __attribute__((cheri_method_class(PLT_UNIQUE_OBJECT(name))))            \
                        ret name sig;

    #define MAKE_DEFAULT(name, ...) static struct cheri_object PLT_UNIQUE_OBJECT(name);

    #define INIT_OBJ(name, ret, sig, data)    \
        PLT_UNIQUE_OBJECT(name).code = plt_if -> name;  \
        PLT_UNIQUE_OBJECT(name).data = data;

    #define DECLARE_PLT_INIT(type, LIST)                                \
    static inline void init_ ## type (type* plt_if, capability data) {  \
        LIST(INIT_OBJ, data)                                            \
    }

    #define PLT(type, LIST)                 \
    typedef struct                          \
    {                                       \
        LIST(PLT_GOT_ENTRY,)                \
    } type;                                 \
    LIST(MAKE_DEFAULT,)                     \
    LIST(CCALL_WRAP,)                       \
    DECLARE_PLT_INIT(type, LIST)

#else // __ASSEMBLY__

    // If this is included from assembly, we should instead create a set of .set directives that define the offsets
    // In the .plt.got
    .set enum_ctr, 0
    #define SET_NAME(NAME, ...) .set NAME ## _offset, (enum_ctr * CAP_SIZE); .set enum_ctr, enum_ctr + 1;
    #define PLT(type, LIST) LIST(SET_NAME,)

#endif // __ASSEMBLY__

#endif //CHERIOS_CHERIPLT_H
