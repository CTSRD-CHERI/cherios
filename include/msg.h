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
#ifndef CHERIOS_MSG_H
#define CHERIOS_MSG_H

#include "cdefs.h"
#include "ccall.h"
#include "syscalls.h"

//Leftmost arguments are for the message recipient. Other arguments are to the message enqueue function

// We define pretty much the same function in syscall.h, but we do it here again with a capability return type
// This is because message send really returns BOTH c3 and v0/v1.

__BEGIN_DECLS
extern capability message_send_c(register_t a0, register_t a1, register_t a2, register_t a3,
                          capability c3, capability c4, capability c5, capability c6,
                          act_kt dest, ccall_selector_t selector, register_t v0);

typedef struct sync_state_t {
    act_reply_kt sync_caller;
} sync_state_t;

_Static_assert(offsetof(sync_state_t, sync_caller) == 0, "used by assembly");

extern __thread sync_state_t sync_state;

extern __thread long msg_enable;

void next_msg(void);
msg_t* get_message(void);
void pop_msg(msg_t * msg);
int msg_queue_empty(void);

__END_DECLS

#define IS_T_CAP(X) (sizeof(X) == sizeof(void*))

#define NTH_CAP_F(res, arg) ( (((SECOND res) == 0) && IS_T_CAP(arg)) ? (void*)(uintptr_t)(arg) : FIRST res, SECOND res - IS_T_CAP(arg))
// Gets the nth cap of a list of arguments, otherwise NULL
#define NTH_CAPH(N, ...) EVAL16(FOLDl(NTH_CAP_F,(NULL,N), __VA_ARGS__))
#define NTH_CAP(...) EVAL1(FIRST NTH_CAPH(__VA_ARGS__))
#define NTH_NCAP_F(res, arg) ( (((SECOND res) == 0) && !IS_T_CAP(arg)) ? (register_t)(uintptr_t)(arg) : FIRST res, SECOND res - !IS_T_CAP(arg))
// Gets the nth non-cap of a list of arguments, otherwise 0
#define NTH_NCAPH(N, ...) EVAL16(FOLDl(NTH_NCAP_F,(0,N), __VA_ARGS__))
#define NTH_NCAP(...) EVAL1(FIRST NTH_NCAPH(__VA_ARGS__))
#define PASS_CAP(X) (void*)(uintptr_t)X

// Gets the nth arg, or else a default
#ifdef MERGED_FILE

// Just pass the arguments padding to 8. The compiler will sort out casts.
    #define ARG0(X, ...) X
    #define ARG1(a,X, ...) X
    #define ARG2(a,b,X, ...) X
    #define ARG3(a,b,c,X, ...) X
    #define ARG4(a,b,c,d,X, ...) X
    #define ARG5(a,b,c,d,e,X, ...) X

    #define ARG6(a,b,c,d,e,f,X, ...) X
    #define ARG7(a,b,c,d,e,f,g, X, ...) X

    #define ARG_ASSERTS(...) _Static_assert(!IS_T_CAP(ARG4(__VA_ARGS__, 0,0,0,0,0,0,0,0)) && \
                                    !IS_T_CAP(ARG5(__VA_ARGS__, 0,0,0,0,0,0,0,0)) &&         \
                                    !IS_T_CAP(ARG6(__VA_ARGS__, 0,0,0,0,0,0,0,0)) &&         \
                                    !IS_T_CAP(ARG7(__VA_ARGS__, 0,0,0,0,0,0,0,0)), "Merged register file passes only the first four arguments as caps");

    #define MARSHALL_ARGUMENTSH(...) (register_t)ARG4(__VA_ARGS__, 0,0,0,0,0,0,0,0), (register_t)ARG5(__VA_ARGS__, 0,0,0,0,0,0,0,0), (register_t)ARG6(__VA_ARGS__, 0,0,0,0,0,0,0,0), (register_t)ARG7(__VA_ARGS__, 0,0,0,0,0,0,0,0), \
                                    PASS_CAP(ARG0(__VA_ARGS__, 0,0,0,0,0,0,0,0)), PASS_CAP(ARG1(__VA_ARGS__, 0,0,0,0,0,0,0,0)), PASS_CAP((uintptr_t)ARG2(__VA_ARGS__, 0,0,0,0,0,0,0,0)), PASS_CAP(ARG3(__VA_ARGS__, 0,0,0,0,0,0,0,0))
#else
    #define ARG_ASSERTS(...)
// On a split file, we pass the caps separately from the
#define MARSHALL_ARGUMENTSH(...) NTH_NCAP(0, __VA_ARGS__), NTH_NCAP(1, __VA_ARGS__), NTH_NCAP(2, __VA_ARGS__), NTH_NCAP(3, __VA_ARGS__), \
                                PASS_CAP(NTH_CAP(0, __VA_ARGS__)), PASS_CAP(NTH_CAP(1, __VA_ARGS__)), PASS_CAP(NTH_CAP(2, __VA_ARGS__)), PASS_CAP(NTH_CAP(3, __VA_ARGS__))\

#endif
#define MARSHALL_ARGUMENTS(...) IF_ELSE(HAS_ARGS(__VA_ARGS__))(MARSHALL_ARGUMENTSH(__VA_ARGS__))(0,0,0,0,NULL,NULL,NULL,NULL)

#define message_send_marshal_help(f, target, mode, selector, ...) f(MARSHALL_ARGUMENTS(__VA_ARGS__), target, mode, selector)
#define message_send_marshal(...) message_send_marshal_help(message_send, __VA_ARGS__)
#define message_send_marshal_c(...) message_send_marshal_help(message_send_c, __VA_ARGS__)

#define MESSAGE_WRAP_BODY(rt, name, sig, target, method_n)                                                      \
    ARG_ASSERTS(MAKE_ARG_LIST(sig))                                                                             \
    if (IS_T_CAP(rt)) {                                                                                         \
        return (rt)(uintptr_t)message_send_marshal_c(target, SYNC_CALL, method_n MAKE_ARG_LIST_APPEND(sig));    \
    } else {                                                                                                    \
        return (rt)(uintptr_t)message_send_marshal(target, SYNC_CALL, method_n MAKE_ARG_LIST_APPEND(sig));      \
    }

// A message send wrapper that lowers the vargs into the correct slot of a message
#define MESSAGE_WRAP(rt, name, sig, target, method_n)                                                           \
rt name MAKE_SIG(sig) {                                                                                         \
    MESSAGE_WRAP_BODY(rt, name, sig, target, method_n)                                                          \
}

// Same again, but with a default value
#define MESSAGE_WRAP_DEF(rt, name, sig, target, method_n, def_val)                                              \
rt name MAKE_SIG(sig) {                                                                                         \
    if (!target) return (rt)def_val;                                                                            \
    MESSAGE_WRAP_BODY(rt, name, sig, target, method_n)                                                          \
}

// Same again, but will resolve the target if need be, and return a default value if it cannot
#define MESSAGE_WRAP_ID(rt, name, sig, target, method_n, target_num, def_val)                                   \
rt name MAKE_SIG(sig) {                                                                                         \
    if (!target) target = namespace_get_ref(target_num);                                                        \
    if (!target) return (rt)def_val;                                                                            \
    MESSAGE_WRAP_BODY(rt, name, sig, target, method_n)                                                          \
}

#define MESSAGE_WRAP_ID_ASSERT(rt, name, sig, target, method_n, target_num)                                     \
rt name MAKE_SIG(sig) {                                                                                         \
    if (!target) target = namespace_get_ref(target_num);                                                        \
    assert(target);                                                                                             \
    MESSAGE_WRAP_BODY(rt, name, sig, target, method_n)                                                          \
}

#define MESSAGE_WRAP_ID_ASSERT_ERRT(rt, name, sig, target, method_n, target_num)                                \
ERROR_T(rt) name MAKE_SIG(sig) {                                                                                         \
    if (!target) target = namespace_get_ref(target_num);                                                        \
    assert(target);                                                                                             \
    ARG_ASSERTS(MAKE_ARG_LIST(sig))                                                                                  \
    return MAKE_VALID(rt,message_send_marshal_c(target, SYNC_CALL, method_n MAKE_ARG_LIST_APPEND(sig)));        \
}

#endif //CHERIOS_MSG_H
