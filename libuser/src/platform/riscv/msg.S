/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lawrence Esswood
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include "asm.S"
#include "reg_abi.h"
#include "queue.h"
#include "syscalls.h"

// Give some saved registers names
#define timeout             s0
#define flags               s1
#define queue               cs2
#define sync_cap            cs3
#define msg_methods_nb_g    s4
#define ctrl_methods_nb_g   s5
#define msg_table           cs6
#define ctrl_table          cs7
#define self_ctrl_cap       cs8
#define tail                s9

#define stack_spills        cra, cs0, cs1, cs2, cs3, cs4, cs5, cs6, cs7, cs8, cs9

// timeout < 0 waits forever. flags are MSG_ENTRY_TIMEOUT_ON_NOTIFY, MSG_ENTRY_TIMEOUT_ON_MESSAGE
// extern void msg_entry(int64_t timeout, int flags);
// sync_state_cap should hold the sync token

.weak msg_methods
.weak ctrl_methods
.weak msg_methods_nb
.weak ctrl_methods_nb
.hidden msg_methods
.hidden ctrl_methods
.hidden msg_methods_nb
.hidden ctrl_methods_nb

START_FUNC msg_entry

    SPILL_STACK(stack_spills)

    // Put the variables we use in the main loop into saves
    move            timeout, a0
    move            flags, a1
    GET_TLS_SYM_VAL (queue, act_self_queue)
    GET_TLS_SYM     (sync_cap, sync_state)

    GET_SYM_VAL     (msg_methods_nb_g, msg_methods_nb)
    GET_SYM_VAL     (ctrl_methods_nb_g, ctrl_methods_nb)

    GET_SYM         (msg_table, msg_methods)
    GET_SYM         (ctrl_table, ctrl_methods)

    GET_TLS_SYM_VAL (self_ctrl_cap, act_self_ctrl)

    # Set up first 4 arguments to fastpath_wait to not send a return
    li              a3, 0
msg_loop_er:
    li              a0, 0
    li              a1, 0
    li              a2, 0
msg_loop:
    # Calculate if there is something in the queue
    clc             ct0, HEADER_END_OFFSET(queue)
    clw             tail, HEADER_START_OFFSET(queue)
    clw             t0, 0(ct0)
    bne             tail, t0, return_before_pop

    # 0 timeout means immediate return
    beqz            timeout, return_to_caller


    # First three arguments are return arguments from last message (set before loop, or end of last iteration)
    # fastpath_wait, void, (capability c3, register_t v0, register_t v1, act_reply_kt reply_token, int64_t timeout, int notify_is_timeout)
    andi            a5, flags, 1    # MSG_ENTRY_TIMEOUT_ON_NOTIFY
    addi            a4, timeout, 1  # kernel treats 0 as inf, not -1.

    call_func       fastpath_wait
    # a1 will contain an enum:
        # 0: fastpath, registers will already
        # 1: timeout
        # 2: msg in queue
    # First swap it with t2 (so the handle_msg will have its arguments correct)
    swap            a1, t2, t3
    beqz            t2, handle_msg
    add             t2, t2, -1
    li              a3, 0           # dont accidentally send a fastpath return
    neg             t3, t2
    and             timeout, timeout, t3    # timeout = 0 if there was a timeout
    beqz            t2,  msg_loop   # on timeout check messages one more time
    j               pop_msg

return_before_pop:
    beqz            a3, pop_msg
    li              a4, 0
    #  ITEM(message_reply, int, (capability c3, register_t v0, register_t v1, act_reply_kt caller, int hint_switch), __VA_ARGS__)
    call_func       message_reply
pop_msg:
    clw             t3, HEADER_LEN_OFFSET(queue)    # load length
    addi            t3, t3, -1                      # make into mask
    and             t3, t3, tail                    # calculate index
    sll             t3, t3, MSG_LEN_SHIFT           # convert to offset
    cincoffset      ct3, queue, t3                  # msg entry

    # NOTE: This is very sadly broken compared to split register files
    # On split register files it was obvious which arguments were caps, but here it is not
    # We will adopt the convention that capabilities should be the first arguments. This means
    # that only the first 4 arguments in a function can be pointers. Some functions might need
    # their arguments re-ordering because of this.

    clc             ca0, (MSGS_START_OFFSET + 0 * CAP_SIZE)(ct3)
    clc             ca1, (MSGS_START_OFFSET + 1 * CAP_SIZE)(ct3)
    clc             ca2, (MSGS_START_OFFSET + 2 * CAP_SIZE)(ct3)
    clc             ca3, (MSGS_START_OFFSET + 3 * CAP_SIZE)(ct3)
    clc             ct1, (MSGS_START_OFFSET + 4 * CAP_SIZE)(ct3)                    # sync token
    cld             a4,  (MSGS_START_OFFSET + (5 * CAP_SIZE) + (0 * REG_SIZE))(ct3)
    cld             a5,  (MSGS_START_OFFSET + (5 * CAP_SIZE) + (1 * REG_SIZE))(ct3)
    cld             a6,  (MSGS_START_OFFSET + (5 * CAP_SIZE) + (2 * REG_SIZE))(ct3)
    cld             a7,  (MSGS_START_OFFSET + (5 * CAP_SIZE) + (3 * REG_SIZE))(ct3)
    cld             t0,  (MSGS_START_OFFSET + (5 * CAP_SIZE) + (4 * REG_SIZE))(ct3) # selector

    # Update tail
    addi            tail, tail, 1
    csw             tail, HEADER_START_OFFSET(queue)
handle_msg:
    # MSG_ENTRY_TIMEOUT_ON_MESSAGE
    andi            t3, flags, 2
    beqz            t3, 1f
    li              timeout, 0
1:
    cmove           ct3, msg_table
    move            t4, msg_methods_nb_g
    bgez            t0, make_call               # selectors >=0 are normal messages
control:
    cmove           ct3, ctrl_table
    move            t4, ctrl_methods_nb_g
    neg             t0, t0
make_call:
    bge             t0, t4, overflow
    csc             ct1, 0(sync_cap)            # save sync cap
    sll             t0, t0, CAP_SIZE_BITS
    cincoffset      ct3, ct3, t0
    clc             ct3, 0(ct3)                 # load method
    cjalr           cra, ct3                    # call it
    # Restore sync
    clc             ca3, 0(sync_cap)
    csc             cnull, 0(sync_cap)
    # We split arguments weirdly to match mips. ca0 will be passed direcrly.
    # ca1 could be passed as two ints, currently its integer componant will just be passed in a1.
    li              a2, 0
    j               msg_loop
overflow:
    cmove           ca3, ct1
    j               msg_loop_er
return_to_caller:
    # Might still have a mesage to return
    beqz            a3, pop_frame
    li              a4, 0
    #  ITEM(message_reply, int, (capability c3, register_t v0, register_t v1, act_reply_kt caller, int hint_switch), __VA_ARGS__)
    call_func       message_reply
pop_frame:
    RESTORE_STACK(stack_spills)
    cret
END_FUNC msg_entry
