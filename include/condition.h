/*-
 * Copyright (c) 2020 Lawrence Esswood
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
#ifndef CHERIOS_CONDITION_H
#define CHERIOS_CONDITION_H

#include "cheric.h"
#include "syscalls.h"

/* Some helper functions to implement a conditional variable.
 * These are primarily for the socket library, but have been hoisted out here as they are generally useful.
 * Condition variables are a pair of condition_t* (a monitored integer) and
 * a act_notify_kt* (a slot to store a token to be notified if the variable changes in any way) */

typedef uint16_t condition_t;
typedef uint8_t cancel_t;

#define CONDITION_CANCELLED (-6) // please don't change this. Its designed to fit in with socket errors
#define CONDITION_SHOULD_SLEEP 1 // please don't change this. Assembly uses this value.

#include "condition_platform.h"

/* Sets a condition and notifies anybody waiting on it.
 * NOT safe to call if any of the pointed to memory might be unmapped. */
static inline int condition_set_and_notify(volatile condition_t* ptr, condition_t new_val, volatile act_notify_kt* waiter_cap) {
    act_notify_kt waiter = condition_set(ptr, new_val, waiter_cap);

    if(waiter) {
        *waiter_cap = NULL;
        syscall_cond_notify(waiter);
    }

    return 0;
}



// A non-exhaustive set of wrappers for functions you may want.
// If you don't find what you need, use the underlying function.

// Sleep for *monitor == comp_val

static inline int condition_sleep_for_equal(volatile act_notify_kt* wait_cap, volatile cancel_t* cancelled_cap,
                                            volatile condition_t* monitor_cap, condition_t comp_val, int delay_sleep, act_notify_kt n_token) {
    return condition_sleep_for_condition(wait_cap, cancelled_cap, monitor_cap, comp_val+1, 0xFFFF, delay_sleep, n_token);
}

// Sleep for *monitor != comp_val

static inline int condition_sleep_for_not_equal(volatile act_notify_kt* wait_cap, volatile cancel_t* cancelled_cap,
                                  volatile condition_t* monitor_cap, condition_t comp_val, int delay_sleep, act_notify_kt n_token) {
    return condition_sleep_for_condition(wait_cap, cancelled_cap, monitor_cap, comp_val, 1, delay_sleep, n_token);
}

// Sleep for *monitor >= comp_val

static inline int condition_sleep_for_greater_equal(volatile act_notify_kt* wait_cap, volatile cancel_t* cancelled_cap,
                                                volatile condition_t* monitor_cap, condition_t comp_val, int delay_sleep, act_notify_kt n_token) {
    return condition_sleep_for_condition(wait_cap, cancelled_cap, monitor_cap, 0, comp_val, delay_sleep, n_token);
}

// Sleep for *monitor > comp_val
static inline int condition_sleep_for_greater(volatile act_notify_kt* wait_cap, volatile cancel_t* cancelled_cap,
                                                    volatile condition_t* monitor_cap, condition_t comp_val, int delay_sleep, act_notify_kt n_token) {
    return condition_sleep_for_condition(wait_cap, cancelled_cap, monitor_cap, 0, comp_val+1, delay_sleep, n_token);
}

// Sleep for *monitor <= comp_val
static inline int condition_sleep_for_less_equal(volatile act_notify_kt* wait_cap, volatile cancel_t* cancelled_cap,
                                              volatile condition_t* monitor_cap, condition_t comp_val, int delay_sleep, act_notify_kt n_token) {
    // m - (comp_val+1) >= (0xFFFF - comp_val)
    return condition_sleep_for_condition(wait_cap, cancelled_cap, monitor_cap, comp_val+1, 0xFFFF - comp_val, delay_sleep, n_token);
}

// Sleep for *monitor < comp_val
static inline int condition_sleep_for_less(volatile act_notify_kt* wait_cap, volatile cancel_t* cancelled_cap,
                                              volatile condition_t* monitor_cap, condition_t comp_val, int delay_sleep, act_notify_kt n_token) {
    // m - (comp_val) >= (0x10000 - comp_val)
    return condition_sleep_for_condition(wait_cap, cancelled_cap, monitor_cap, comp_val, 0x10000 - comp_val, delay_sleep, n_token);
}

// The compiler things it can get rid of the &0xFFFF when it cannot. Use assembly to fool it.
#define TRUNCATE16(X) ({uint16_t _tmpx; __asm ("andi   %[out], %[in], 0xFFFF\n":[out]"=r"(_tmpx):[in]"r"(X):); _tmpx;})

// Just a really useful condition for the socket library which does buffer_size - (request - *fulfil) >= space_needed
// Sleep for C1 - (C2 - *monitor) >= C3
static inline int condition_sleep_for_condition2(volatile act_notify_kt* wait_cap, volatile cancel_t* cancelled_cap,
                                              volatile condition_t* monitor_cap, condition_t c1, condition_t c2, condition_t  c3,
                                              int delay_sleep, act_notify_kt n_token) {
    condition_t amount = ((~(c1 - c3)));
    amount = TRUNCATE16(amount);
    return condition_sleep_for_condition(wait_cap, cancelled_cap, monitor_cap, c2 + 1, amount, delay_sleep, n_token);
}

#endif //CHERIOS_CONDITION_H
