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

#ifndef CHERIOS_STRING_ENUMS_H_H
#define CHERIOS_STRING_ENUMS_H_H

#define SE_ENUM_ELEMENT1(name) name,
#define SE_ENUM_ELEMENT2(name, val) name = val,

#define SE_ENUM_ELEMENT_SELECT_MACRO(_1, _2, NAME, ...) NAME
#define SE_ENUM_ELEMENT_SELECT(...) \
     SE_ENUM_ELEMENT_SELECT_MACRO(__VA_ARGS__, SE_ENUM_ELEMENT2, SE_ENUM_ELEMENT1)(__VA_ARGS__)

#define SE_ARRAY_ELEMENT(name) #name,
#define SE_CASE_ELEMENT(name, val) case val: return #name;


#define DECLARE_ENUM(Type, LIST_DEF)                  \
    typedef enum Type {                               \
        LIST_DEF(SE_ENUM_ELEMENT_SELECT)              \
    } Type;                                           \
    const char* enum_ ## Type ## _tostring(Type val); \

#ifndef __LITE__
    #define ARRAY_COUNT(A) sizeof(A)/sizeof(A[0])
    #define DEFINE_ENUM_AR(Type, LIST_DEF)                  \
        static const char* str_table_ ## Type [] = {        \
            LIST_DEF(SE_ARRAY_ELEMENT)                      \
        };                                                  \
        const char* enum_ ## Type ## _tostring(Type val) {  \
            if((uint32_t)val >= ARRAY_COUNT(str_table_ ## Type))       \
                return "INVALID " #Type;                    \
            return str_table_ ## Type [val];                \
        }                                                   \

    #define DEFINE_ENUM_CASE(Type, LIST_DEF)                \
        const char* enum_ ## Type ## _tostring(Type val) {  \
            switch(val) {                                   \
            LIST_DEF(SE_CASE_ELEMENT)                       \
            default: return "INVALID " #Type;               \
            }                                               \
        }                                                   \

#else

    #define DEFINE_ENUM_AR(Type, LIST_DEF)                  \
        const char* enum_ ## Type ## _tostring(Type val) {  \
            return "";                                      \
        }                                                   \

    #define DEFINE_ENUM_CASE(Type, LIST_DEF)                \
        const char* enum_ ## Type ## _tostring(Type val) {  \
            return "";                                      \
        }                                                   \

#endif

#endif //CHERIOS_STRING_ENUMS_H_H
