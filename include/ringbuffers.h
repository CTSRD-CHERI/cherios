/*-
 * Copyright (c) 2018 Lawrence Esswood
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
#ifndef CHERIOS_RINGBUFFERS_H
#define CHERIOS_RINGBUFFERS_H

/* An example of declaring a statically sized ringbuffer of ints, of size 32, indexed by uin8_t
 * inside a struct called foo:

#define MY_RB(...) RINGBUF_MEMBER_STATIC(my_ring,int,uint8_t,32,__VA_ARGS__);

struct foo {
    ...
    RINGBUF_DEF_FIELDS_STATIC(MY_RB);
    ...
    RINGBUF_DEF_BUF_STATIC(MY_RB);
};

 * And now use it like this:

    struct foo* my_foo = ...
    *RINGBUF_PUSH(MY_RB, my_foo) = 1;
    *RINGBUF_PUSH(MY_RB, my_foo) = 2;
    *RINGBUF_PUSH(MY_RB, my_foo) = 3;
    RINGBUF_FOREACH_POP(x, MY_RB, my_foo) {
        bar(x); // call bar on each item and them pop that item
    }

 */


// Use these to define a new ringbuf definition. Provide this to al the other macros to customise how they behave

// Usage #define MY_RB(...) RINGBUF_[MEMBER|LOC|PTRS]_[STATIC|DYNAMIC](lower_case_args, __VA_ARGS__)

// MEMBER: As a member of a struct. Provide the instance of the struct as an additional argument.
// LOC: On the stack / as globals.
// PTRS: As a bunch of pointers.

#define RINGBUF_MEMBER_STATIC(name,t,id_t,sz, N, T, ID_T, SZ, BUF, HD, TL, parent, ...) \
    N(name, __VA_ARGS__)                        \
    T(t, __VA_ARGS__)                           \
    ID_T(id_t, __VA_ARGS__)                     \
    SZ(sz, __VA_ARGS__)                         \
    BUF((parent)-> name ## _buf, __VA_ARGS__)   \
    HD((parent) -> name ## _hd, __VA_ARGS__)    \
    TL((parent) -> name ## _tl, __VA_ARGS__)

#define RINGBUF_MEMBER_DYNAMIC(name,t,id_t, N, T, ID_T, SZ, BUF, HD, TL, parent, ...) \
    N(name, __VA_ARGS__)                        \
    T(t, __VA_ARGS__)                           \
    ID_T(id_t, __VA_ARGS__)                     \
    SZ((parent)-> name ## _sz, __VA_ARGS__)     \
    BUF((parent)-> name ## _buf, __VA_ARGS__)   \
    HD((parent) -> name ## _hd, __VA_ARGS__)    \
    TL((parent) -> name ## _tl, __VA_ARGS__)

#define RINGBUF_LOC_STATIC(name,t,id_t,sz, N, T, ID_T, SZ, BUF, HD, TL, ...) \
    N(name, __VA_ARGS__)                        \
    T(t, __VA_ARGS__)                           \
    ID_T(id_t, __VA_ARGS__)                     \
    SZ(sz, __VA_ARGS__)                         \
    BUF(name ## _buf, __VA_ARGS__)              \
    HD(name ## _hd, __VA_ARGS__)                \
    TL(name ## _tl, __VA_ARGS__)

#define RINGBUF_LOC_DYNAMIC(name,t,id_t, N, T, ID_T, SZ, BUF, HD, TL, ...) \
    N(name, __VA_ARGS__)                        \
    T(t, __VA_ARGS__)                           \
    ID_T(id_t, __VA_ARGS__)                     \
    SZ(name ## _sz, __VA_ARGS__)                \
    BUF(name ## _buf, __VA_ARGS__)              \
    HD(name ## _hd, __VA_ARGS__)                \
    TL(name ## _tl, __VA_ARGS__)

#define RINGBUF_PTRS(name,t,id_t,sz, buf, hd, tl, N, T, ID_T, SZ, BUF, HD, TL, ...) \
    N(name, __VA_ARGS__)                        \
    T(t, __VA_ARGS__)                           \
    ID_T(id_t, __VA_ARGS__)                     \
    SZ(sz, __VA_ARGS__)                         \
    BUF(buf, __VA_ARGS__)                       \
    HD(*(hd), __VA_ARGS__)                      \
    TL(*(tl), __VA_ARGS__)

// Use these to define your ringbufs either globally / on the stack / in a structure

#define RINGBUF_DEF_BUF_STATIC(RB,...) \
    RINGBUF_T(RB,__VA_ARGS__, )  RBCAT(RINGBUF_NAME(RB,__VA_ARGS__, ), _buf) [RINGBUF_SZ(RB,__VA_ARGS__, )]

#define RINGBUF_DEF_BUF_ZERO(RB,...) RINGBUF_(RB,__VA_ARGS__, )  RINGBUF_NAME(RB,__VA_ARGS__, ) \
    RINGBUF_T(RB,__VA_ARGS__, )  RBCAT(RINGBUF_NAME(RB,__VA_ARGS__, ), _buf)[]

#define RINGBUF_DEF_BUF_DYN(RB,...) RINGBUF_(RB,__VA_ARGS__,)  RINGBUF_NAME(RB,__VA_ARGS__,) \
    RINGBUF_T(RB,__VA_ARGS__,)*  RBCAT(RINGBUF_NAME(RB,__VA_ARGS__,),_buf)

#define RINGBUF_DEF_FIELDS_STATIC(RB,...) \
    RINGBUF_INDEX_T(RB,__VA_ARGS__,) RBCAT(RINGBUF_NAME(RB,__VA_ARGS__), _hd); \
    RINGBUF_INDEX_T(RB,__VA_ARGS__,) RBCAT(RINGBUF_NAME(RB,__VA_ARGS__), _tl)

#define RINGBUF_DEF_FIELDS_DYNAMIC(RB,...)      \
    RINGBUF_DEF_FIELDS_STATIC(RB, __VA_ARGS__); \
    RINGBUF_INDEX_T(RB,__VA_ARGS__,) RBCAT(RINGBUF_NAME(RB,__VA_ARGS__), _sz)

// Use these to get fields

#define RINGBUF_NAME(RB,...)    RB(RB_GET,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE, __VA_ARGS__)
#define RINGBUF_T(RB,...)       RB(RB_IGNORE,RB_GET,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE, __VA_ARGS__)
#define RINGBUF_INDEX_T(RB,...) RB(RB_IGNORE,RB_IGNORE,RB_GET,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE, __VA_ARGS__)
#define RINGBUF_SZ(RB,...)      RB(RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_GET,RB_IGNORE,RB_IGNORE,RB_IGNORE, __VA_ARGS__)
#define RINGBUF_BUF(RB,...)     RB(RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_GET,RB_IGNORE,RB_IGNORE, __VA_ARGS__)
#define RINGBUF_HD(RB,...)      RB(RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_GET,RB_IGNORE, __VA_ARGS__)
#define RINGBUF_TL(RB,...)      RB(RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_IGNORE,RB_GET, __VA_ARGS__)

// Operations

#define RINGBUF_EL(n, RB, ...) &(RINGBUF_BUF(RB, __VA_ARGS__)[n & (RINGBUF_SZ(RB, __VA_ARGS__) - 1)])

#define RINGBUF_FILL(RB,...)    ((RINGBUF_TL(RB,__VA_ARGS__))-(RINGBUF_HD(RB,__VA_ARGS__)))
#define RINGBUF_SPACE(RB,...)   ((RINGBUF_SZ(RB, __VA_ARGS__)) - (RINGBUF_FILL(RB,__VA_ARGS__)))
#define RINGBUF_EMPTY(RB,...)    (RINGBUF_TL(RB,__VA_ARGS__) == RINGBUF_HD(RB, __VA_ARGS__))
#define RINGBUF_FULL(RB,...)   ((RINGBUF_SZ(RB, __VA_ARGS__)) == (RINGBUF_FILL(RB,__VA_ARGS__)))

#define RINGBUF_PUSH(RB,...) \
        ((RINGBUF_FULL(RB,__VA_ARGS__)) ? \
        NULL : \
        RINGBUF_EL(RINGBUF_TL(RB,__VA_ARGS__)++,RB,__VA_ARGS__))

#define RINGBUF_POP(RB,...)  RINGBUF_EL((RINGBUF_HD(RB,__VA_ARGS__))++, RB, __VA_ARGS__)
#define RINGBUF_PEEK(RB,...) RINGBUF_EL((RINGBUF_HD(RB,__VA_ARGS__)), RB, __VA_ARGS__)

#define RINGBUF_FOREACH(x, RB, ...) \
    for(size_t rb_start = RINGBUF_HD(RB, __VA_ARGS__),                          \
        size_t rb_end = RINGBUF_TL(RB, __VA_ARGS__),                            \
        RINGBUF_T(RB,__VA_ARGS__)* x = RINGBUF_EL(rb_start, RB, __VA_ARGS__);   \
        rb_start != rb_end;                                                     \
        rb_start++, x = RINGBUF_EL(rb_start, RB, __VA_ARGS__))


#define RINGBUF_FOREACH_POP(x, RB, ...)                                       \
    for(RINGBUF_T(RB,__VA_ARGS__)* x = RINGBUF_EL(RINGBUF_HD(RB, __VA_ARGS__), RB, __VA_ARGS__);   \
        RINGBUF_HD(RB, __VA_ARGS__) != RINGBUF_TL(RB, __VA_ARGS__);             \
        RINGBUF_HD(RB, __VA_ARGS__)++, x = RINGBUF_EL(RINGBUF_HD(RB, __VA_ARGS__), RB, __VA_ARGS__))

// These are helpers

#define RB_GET(X, ...)X
#define RB_IGNORE(...)
#define RBCAT_HLP(A,B) A ## B
#define RBCAT(A,B) RBCAT_HLP(A,B)

#endif //CHERIOS_RINGBUFFERS_H
