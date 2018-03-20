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

#include <queue.h>
#include <sockets.h>
#include "object.h"
#include "string.h"
#include "sockets.h"
#include "stdio.h"

static int is_empty(uni_dir_socket_requester* requester) {
    return (requester->fulfiller_component.fulfill_ptr == requester->requeste_ptr);
}

static int is_full(uni_dir_socket_requester* requester) {
    return ((requester->requeste_ptr - requester->fulfiller_component.fulfill_ptr) == requester->buffer_size);
}

static uint16_t fill_level(uni_dir_socket_requester* requester) {
    return (requester->requeste_ptr - requester->fulfiller_component.fulfill_ptr);
}

static uint16_t space(uni_dir_socket_requester* requester) {
    return (requester->buffer_size - (requester->requeste_ptr - requester->fulfiller_component.fulfill_ptr));
}

static uint16_t space_from_fulfill_ptr(uni_dir_socket_requester* requester, uint16_t fulfill_ptr) {
    return (requester->buffer_size - (requester->requeste_ptr - fulfill_ptr));
}

static size_t data_buf_space(data_ring_buffer* data_buffer) {
    return (data_buffer->buffer_size - (data_buffer->requeste_ptr - data_buffer->fulfill_ptr));
}

int socket_internal_requester_space_wait(uni_dir_socket_requester* requester, uint16_t need_space, int dont_wait) {

    if(requester->fulfiller_component.fulfiller_closed || requester->requester_closed) {
        return E_SOCKET_CLOSED;
    }

    int full = space(requester) <= need_space;

    if(!full) return 0;

    if(full && dont_wait) return E_AGAIN;

    volatile uint16_t* fulfill_ptr_cap = &(requester->fulfiller_component.fulfill_ptr);
    volatile act_kt* requester_waiting_cap = &requester->fulfiller_component.requester_waiting;
    uint16_t requeste_ptr = requester->requeste_ptr;
    uint16_t check =  (requester->buffer_size) - need_space; // This much allowed to be taken

    while(full) {
        // Gives 1 if full (and sets waiting), 2 if closed, 0 otherwise
        __asm__ __volatile(
        SANE_ASM
        "2:      cllc    $c1, %[waiting_cap]        \n"     // Load linked waiting
                "clb    $at, $zero, 0(%[cc])        \n"
                "bnez   $at, 1f                     \n"     // Check fulfiller didn't close the socket
                "li     %[res], 2                   \n"
                "clhu   $at, $zero, 0(%[fulfill_ptr_cap])\n"   // Load fulfill ptr
                "subu   $at, %[req], $at            \n"
                "andi   $at, $at, (1<<16)-1         \n"     // How much space is taken
                "sltu   $at, %[check_val], $at      \n"     // 0 if enough space
                "beqz   $at, 1f                     \n"     // If not full go to next step
                "li     %[res], 0                   \n"
                "cscc   $at, %[self], %[waiting_cap] \n" // Try set in waiting state
                "beqz   $at, 2b                     \n"     // If we failed to requeste the cap, try all this again
                "li     %[res], 1                   \n"
                "1:                                 \n"
        : [res]"=r"(full)
        : [waiting_cap]"C"(requester_waiting_cap),
        [self]"C"(act_self_notify_ref),
        [fulfill_ptr_cap]"C"(fulfill_ptr_cap),
        [check_val]"r"(check),
        [req]"r"(requeste_ptr),
        [cc]"C"(&requester->fulfiller_component.fulfiller_closed)
        : "$c1", "at"
        );

        if(full == 2) {
            return E_SOCKET_CLOSED;
        }
        if(full) syscall_cond_wait();
    }

    return 0;
}

// Call space wait first!

int socket_internal_requester_request(uni_dir_socket_requester* requester, char* buffer, int dont_wait) {

    uint16_t requester_ptr = requester->requeste_ptr;

    // Put a cap in the buffer
    requester->indirect_ring_buffer[requester_ptr & (requester->buffer_size-1)] = buffer;


    uint16_t requester_ptr_new = (requester_ptr + 1);
    volatile uint16_t* request_cap = &requester->requeste_ptr;
    // Updates requeste_ptr. Will cause fulfiller to fail setting waiting on race. Returns the waiter (if any)
    act_kt waiter;
    volatile act_kt* waiter_cap = &requester->fulfiller_component.fulfiller_waiting;
    __asm__ __volatile(
            "cllc   %[res], %[waiting_cap]                     \n"
            "csh    %[new_requeste], $zero, 0(%[new_cap])      \n"
            "cscc   $at, %[res], %[waiting_cap]                \n"
            "clc    %[res], $zero, 0(%[waiting_cap])           \n"
    : [res]"=C"(waiter),
    [new_cap]"+C"(request_cap) // This is listed as an output because otherwise it seems to get clobered...
    : [waiting_cap]"C"(waiter_cap),
    [new_requeste]"r"(requester_ptr_new)
    : "at"
    );

    if(waiter) {
        *waiter_cap = NULL;
        syscall_cond_notify(waiter);
    }

    return 0;
}

int socket_internal_fulfill_peek(uni_dir_socket_fulfiller* fulfiller, uint16_t amount, int dont_wait) {
    uni_dir_socket_requester_fulfiller_component* access = fulfiller->requester->access;

    if(fulfiller->requester->requester_closed || access->fulfiller_closed) {
        return E_SOCKET_CLOSED;
    }

    int empty = fill_level(fulfiller->requester) < amount;

    if(!empty) return 0;

    if(dont_wait) return E_AGAIN;

    volatile act_kt* fulfiller_waiting_cap = &access->fulfiller_waiting;
    volatile uint16_t* requeste_ptr_cap = &(fulfiller->requester->requeste_ptr);
    uint16_t check = amount;

    while(empty) {
        // Gives 1 if not full enough and sets waiting
        __asm__ __volatile(
        SANE_ASM
        "2:      cllc   $c1, %[waiting_cap]                 \n" // Load linked waiting
                "clb    $at, $zero, 0(%[cc])                \n"
                "bnez   $at, 1f                             \n" // Check requester didn't close the socket
                "li     %[res], 2                           \n"
                "clhu   %[res], $zero, 0(%[requeste_ptr_cap])  \n" // Load request ptr
                "subu   %[res], %[res], %[fulfi]            \n"
                "andi   %[res], %[res], (1<<16)-1           \n"     // How much has been requested
                "sltu   %[res], %[res], %[check_val]        \n" // 1 if: requested < amount
                "beqz   %[res], 1f                          \n" // If enough go to next step
                "nop                                        \n"
                "cscc   %[res], %[self], %[waiting_cap]     \n"  // Try set in waiting state
                "beqz   %[res], 2b                          \n"     // If we failed to requeste the cap, try all this again
                "li     %[res], 1                           \n"
        "1:\n"
        : [res]"+r"(empty)
        : [waiting_cap]"C"(fulfiller_waiting_cap),
        [self]"C"(act_self_notify_ref),
        [requeste_ptr_cap]"C"(requeste_ptr_cap),
        [fulfi]"r"(access->fulfill_ptr),
        [check_val]"r"(check),
        [cc]"C"(&fulfiller->requester->requester_closed)
        : "$c1"
        );

        if(empty == 2) {
            return E_SOCKET_CLOSED;
        }

        if(empty) syscall_cond_wait();
    }

    return 0;
}

// Progresses fulfill_ptr. Call peek FIRST.

int socket_internal_fulfill_progress(uni_dir_socket_fulfiller* fulfiller, uint16_t amount, int dont_wait) {

    uni_dir_socket_requester_fulfiller_component* access = fulfiller->requester->access;

    uint16_t fulfill_ptr_new = access->fulfill_ptr + amount;

    // Updates fulfill_ptr. Will cause requester to fail setting waiting on race. Returns the waiter (if any)
    act_kt waiter;
    __asm__ __volatile(
        "cllc   %[res], %[waiting_cap]                     \n"
        "csh    %[new_fulfill], $zero, 0(%[new_fulfill_cap])     \n"
        "cscc   $at, %[res], %[waiting_cap]                \n"
        "clc    %[res], $zero, 0(%[waiting_cap])           \n"
    : [res]"=C"(waiter)
    : [waiting_cap]"C"(&access->requester_waiting),
      [new_fulfill_cap]"C"(&access->fulfill_ptr),
      [new_fulfill]"r"(fulfill_ptr_new)
    : "at"
    );

    if(waiter) {
        access->requester_waiting = NULL;
        syscall_cond_notify(waiter);
    }

    return 0;
}

ssize_t socket_internal_fulfill(uni_dir_socket_fulfiller* fulfiller, char* buf, size_t length, enum SOCKET_FLAGS flags) {
    int res;

    uni_dir_socket_requester* requester = fulfiller->requester;
    uint16_t mask = requester->buffer_size - 1;
    uint16_t fulfill_ptr = requester->fulfiller_component.fulfill_ptr;
    int socket_is_pull = requester->socket_type == SOCK_TYPE_PULL;

    if(requester->requester_closed || requester->fulfiller_component.fulfiller_closed) {
        return E_SOCKET_CLOSED;
    }

    if(length == 0) return 0;

    register_t force_request_type;

    if(socket_is_pull) {
        if((flags & MSG_NO_CAPS)) {
            buf = cheri_andperm(buf, CHERI_PERM_LOAD);
        }
        force_request_type = CHERI_PERM_STORE | CHERI_PERM_STORE_CAP;
    } else {
        if((flags & MSG_NO_CAPS)) {
            force_request_type = CHERI_PERM_LOAD;
        } else {
            force_request_type = CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP;
        }
    }

    size_t bytes_left = length;

    size_t partial_bytes = fulfiller->partial_fulfill_bytes;

    while(bytes_left != 0) {

        // If this is non zero then we will have at least one capability usable
        if(partial_bytes == 0) {
            res = socket_internal_fulfill_peek(fulfiller, 1, flags & MSG_DONT_WAIT);
            if(res < 0) break;
        }

        char* cap = requester->indirect_ring_buffer[fulfill_ptr & mask];

        cap = cheri_andperm(cap, force_request_type);

        size_t cap_len = cheri_getlen(cap);

        size_t effective_length = cap_len - partial_bytes;

        size_t to_copy;
        size_t new_partial;

        if(effective_length > bytes_left) {
            to_copy = bytes_left;
            new_partial = partial_bytes + bytes_left;
        } else {
            to_copy = effective_length;
            new_partial = 0;
        }

        char* cap1 = buf + length - bytes_left;
        char* cap2 = cap + partial_bytes;

        char* dest = socket_is_pull ? cap2 : cap1;
        char* src = socket_is_pull ? cap1 : cap2;

        memcpy(dest, src, to_copy);

        bytes_left -= to_copy;
        partial_bytes = new_partial;

        if(partial_bytes == 0) {
            fulfill_ptr++;
            // TODO we really could progress multiple caps at a time, but for a large request this might fill up the buffer
            res = socket_internal_fulfill_progress(fulfiller, 1, (flags & MSG_DONT_WAIT));
            if(res < 0) break;
        }
    }

    fulfiller->partial_fulfill_bytes = partial_bytes;
    ssize_t actually_fulfill = length - bytes_left;

    return (actually_fulfill == 0) ? res : actually_fulfill;
}

// Brings the data buffer fulfill pointer back in line.
static uint16_t socket_internal_data_buffer_progress(data_ring_buffer* data_buffer, uni_dir_socket_requester* requester) {

    // TODO need a sync here?
    uint16_t process_to = requester->fulfiller_component.fulfill_ptr;

    if(data_buffer->last_processed_fulfill_ptr != process_to) {
        // We need to reclaim up to this capability

        uint16_t last_fulfill = (process_to - 1) & (requester->buffer_size-1);

        volatile char* last_fulfill_cap = requester->indirect_ring_buffer[last_fulfill];

        size_t mask = data_buffer->buffer_size-1;

        size_t ends_at = (cheri_getbase(last_fulfill_cap) + cheri_getlen(last_fulfill_cap)) - cheri_getbase(data_buffer->buffer);

        size_t old_fulfill_ptr = data_buffer->fulfill_ptr;
        size_t new_fulfill_ptr = (old_fulfill_ptr & ~mask) | ends_at;

        if(new_fulfill_ptr <= old_fulfill_ptr) new_fulfill_ptr+= data_buffer->buffer_size;

        data_buffer->fulfill_ptr = new_fulfill_ptr;
        data_buffer->last_processed_fulfill_ptr = process_to;
    }

    return process_to;
}

static ssize_t socket_internal_data_buffer_to_push_request
        (const char* buf, size_t size,
        data_ring_buffer* data_buffer, uni_dir_socket_requester* requester,
        int dont_wait, register_t perms) {
    int res;

    if(requester->socket_type == SOCK_TYPE_PULL) return E_SOCKET_WRONG_TYPE;

    if(size == 0) return 0;

    size_t mask = data_buffer->buffer_size-1;

    size_t copy_from = (data_buffer->requeste_ptr);

    size_t align_mask = sizeof(capability)-1;
    size_t buf_align = ((size_t)buf) & align_mask;
    size_t data_buf_align = copy_from & align_mask;
    size_t extra_to_align = 0;

    if(size >= sizeof(capability)) {
        extra_to_align = (buf_align - data_buf_align) & align_mask;
    }

    copy_from = (copy_from + extra_to_align) & mask;

    size_t part_1 = data_buffer->buffer_size - copy_from;

    int two_parts = part_1 < size;

    res = socket_internal_requester_space_wait(requester, two_parts ? 2 : 1, dont_wait);

    if(res < 0 ) return res;

    // The request buffer and data buffer keep falling out of sync. Bring back in line.
    uint16_t proc_to = socket_internal_data_buffer_progress(data_buffer, requester);

    size_t data_space = data_buf_space(data_buffer);

    size += extra_to_align;
    // Can't requeste this message because the data buffer does not have enough space
    if(data_space < size && dont_wait) return E_AGAIN;

    // Reclaim data buffer space until there is enough to copy
    uint16_t requeste_ptr = requester->requeste_ptr;
    while (data_space <= size) {
        uint16_t fulfill_ptr = requester->fulfiller_component.fulfill_ptr;

        if(fulfill_ptr == proc_to) {
            uint16_t proc_space = requeste_ptr - fulfill_ptr;
            // Wait for space to increase by at least 1
            res = socket_internal_requester_space_wait(requester, proc_space+1, dont_wait);

            if(res < 0) return res;
        }

        proc_to = socket_internal_data_buffer_progress(data_buffer, requester);
        data_space = data_buf_space(data_buffer);
    }

    size-=extra_to_align;
    data_buffer->requeste_ptr += extra_to_align;

    // We are okay to make a requeste. First copy to buffer and then send those bytes.
    part_1 = two_parts ? part_1 : size;

    memcpy(data_buffer->buffer + copy_from, buf, part_1);
    char* cap1 = cheri_setbounds(data_buffer->buffer + copy_from, part_1);
    cap1 = cheri_andperm(cap1, perms);
    res = socket_internal_requester_request(requester, cap1, dont_wait);
    if(res < 0) return res;
    data_buffer->requeste_ptr+=part_1;

    if(two_parts) {
        size_t part_2 = size - part_1;
        memcpy(data_buffer->buffer, buf + part_1, part_2);
        char* cap2 = cheri_setbounds(data_buffer->buffer, part_2);
        cap2 = cheri_andperm(cap2, perms);
        res = socket_internal_requester_request(requester, cap2, dont_wait);
        if(res < 0) return res;
        data_buffer->requeste_ptr+=part_2;
    }

    return size;
}

int socket_internal_fulfiller_init(uni_dir_socket_fulfiller* fulfiller, uint8_t socket_type) {
    fulfiller->requester = NULL;
    fulfiller->partial_fulfill_bytes = 0;
    fulfiller->socket_type = socket_type;
    return 0;
}

int socket_internal_requester_init(uni_dir_socket_requester* requester, uint16_t buffer_size, uint8_t socket_type) {
    if(!is_power_2(buffer_size)) return E_BUFFER_SIZE_NOT_POWER_2;

    bzero(requester, SIZE_OF_request(buffer_size));
    requester->buffer_size = buffer_size;
    requester->socket_type = socket_type;
    uni_dir_socket_requester_fulfiller_component* access = &(requester->fulfiller_component);
    access = cheri_setbounds(access, sizeof(uni_dir_socket_requester_fulfiller_component));
    requester->access = access;

    return 0;
}


int socket_internal_listen(register_t port,
                           uni_dir_socket_requester* requester,
                           uni_dir_socket_fulfiller* fulfiller) {

    if(requester == NULL && fulfiller == NULL) return E_SOCKET_NO_DIRECTION;

    // THEIR connection type
    enum socket_connect_type con_type = CONNECT_NONE;
    capability ro_req = NULL;
    if(requester) {
        ro_req = (capability)cheri_andperm(requester, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
        if(requester->socket_type == SOCK_TYPE_PULL) {
            con_type |= CONNECT_PULL_WRITE;
        } else {
            con_type |= CONNECT_PUSH_READ;
        }
    }

    if(fulfiller) {
        if(fulfiller->socket_type == SOCK_TYPE_PULL) {
            con_type |= CONNECT_PULL_READ;
        } else {
            con_type |= CONNECT_PUSH_WRITE;
        }
    }

    msg_t msg;
    pop_msg(&msg);

    if(msg.v0 != SOCKET_CONNECT_IPC_NO) {
        return E_CONNECT_FAIL;
    }

    if(msg.a0 != port) {
        message_reply((capability)E_CONNECT_FAIL_WRONG_PORT,0,0,msg.c2, msg.c1);
        return (E_CONNECT_FAIL_WRONG_PORT);
    }

    if(msg.a1 != con_type) {
        // Send failure
        message_reply((capability)E_CONNECT_FAIL_WRONG_TYPE,0,0,msg.c2, msg.c1);
        return (E_CONNECT_FAIL_WRONG_TYPE);
    }

    if(fulfiller) {
        if(msg.c3 == NULL) {
            message_reply((capability)E_CONNECT_FAIL,0,0,msg.c2, msg.c1);
            return (E_CONNECT_FAIL);
        }
        fulfiller->requester = (uni_dir_socket_requester*) msg.c3;
    }

    // ACK receipt
    message_reply((capability)ro_req,0,0,msg.c2, msg.c1);

    // TODO should we add an extra ack?

    return 0;
}

int socket_internal_connect(act_kt target, register_t port,
                            uni_dir_socket_requester* requester,
                            uni_dir_socket_fulfiller* fulfiller) {
    if(requester == NULL && fulfiller == NULL) return E_SOCKET_NO_DIRECTION;

    msg_t msg;

    enum socket_connect_type con_type = CONNECT_NONE;
    capability ro_req = NULL;

    if(requester) {
        ro_req = (capability)cheri_andperm(requester, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
        if(requester->socket_type == SOCK_TYPE_PULL) {
            con_type |= CONNECT_PULL_READ;
        } else {
            con_type |= CONNECT_PUSH_WRITE;
        }
    }

    if(fulfiller) {
        if(fulfiller->socket_type == SOCK_TYPE_PULL) {
            con_type |= CONNECT_PULL_WRITE;
        } else {
            con_type |= CONNECT_PUSH_READ;
        }
    }

    capability cap_result = message_send_c(port,con_type,0,0,(capability)ro_req, NULL, NULL, NULL, target, SYNC_CALL, SOCKET_CONNECT_IPC_NO);

    ERROR_T(requester_ptr_t) res = ER_T_FROM_CAP(requester_ptr_t, cap_result);

    // Check ACK
    if(!IS_VALID(res)) return (int)res.er;

    if(fulfiller) {
        if(res.val == NULL) {
            return E_CONNECT_FAIL;
        }
        fulfiller->requester = (uni_dir_socket_requester*) res.val;
    }

    return 0;
}

static int socket_internal_close_safe(volatile uint8_t* own_close, volatile uint8_t* other_close, volatile act_notify_kt * waiter_cap) {
    if(*own_close) return E_ALREADY_CLOSED;

    // We need to signal the other end

    *own_close = 1;

    if(*other_close) {
        return 0; // If the other has closed on their end they won't expect a signal
    }

    // Otherwise the fulfiller may be about to sleep / alfulfilly be asleep.

    act_notify_kt waiter;

    __asm__ __volatile (
    "cllc   %[res], %[wc]           \n"
            "cscc   $at, %[res], %[wc]      \n"
            "clc    %[res], $zero, 0(%[wc]) \n"
    : [res]"=C"(waiter)
    : [wc]"C"(waiter_cap)
    : "at"
    );

    if(waiter) {
        *waiter_cap = NULL;
        syscall_cond_notify(waiter);
        return 0;
    }

    return 0;
}

int socket_internal_close_requester(uni_dir_socket_requester* requester) {
    return socket_internal_close_safe(&requester->requester_closed,
                                      &requester->fulfiller_component.fulfiller_closed,
                                      &requester->fulfiller_component.fulfiller_waiting);
}

int socket_internal_close_fulfiller(uni_dir_socket_fulfiller* fulfiller) {
    uni_dir_socket_requester_fulfiller_component* access = fulfiller->requester->access;
    return socket_internal_close_safe(&access->fulfiller_closed,
                                      &fulfiller->requester->requester_closed,
                                      &access->requester_waiting);
}


static int init_data_buffer(data_ring_buffer* buffer, char* char_buffer, size_t data_buffer_size) {
    if(!is_power_2(data_buffer_size)) return E_BUFFER_SIZE_NOT_POWER_2;

    buffer->buffer_size = data_buffer_size;
    buffer->buffer = cheri_setbounds(char_buffer, data_buffer_size);
    buffer->fulfill_ptr = 0;
    buffer->requeste_ptr = 0;
    buffer->last_processed_fulfill_ptr = 0;

    return 0;
}

int socket_init(unix_like_socket* sock, enum SOCKET_FLAGS flags,
                char* data_buffer, size_t data_buffer_size,
                enum socket_connect_type con_type) {

    sock->con_type = con_type;

    sock->flags = flags;

    if(data_buffer) {
        return init_data_buffer(&sock->write_copy_buffer, data_buffer, data_buffer_size);
    } else {

        // If no data buffer is provided we may very well wait and cannot perform a copy
        if((flags & MSG_DONT_WAIT)) return E_SOCKET_WRONG_TYPE;
        if((con_type & CONNECT_PUSH_WRITE) && !(flags & MSG_NO_COPY)) return E_COPY_NEEDED;

        return 0;
    }
}

ssize_t socket_requester_request_wait_for_fulfill(uni_dir_socket_requester* requester, char* buf) {
    int ret;

    ret = socket_internal_requester_space_wait(requester, 1, 0);

    if(ret < 0) return ret;

    socket_internal_requester_request(requester, buf, 0);

    // Wait for everything to be consumed
    return socket_internal_requester_space_wait(requester, requester->buffer_size, 0);
}

ssize_t socket_send(unix_like_socket* sock, const char* buf, size_t length, enum SOCKET_FLAGS flags) {
    // Need to copy to buffer in order not to expose buf
    if(sock->con_type & CONNECT_PUSH_WRITE) {
        uni_dir_socket_requester* requester = sock->write.push_writer;
        int dont_wait;

        dont_wait = (flags | sock->flags) & MSG_DONT_WAIT;

        if((sock->flags | flags) & MSG_NO_COPY) {
            if(dont_wait) return E_COPY_NEEDED; // We can't not copy and not wait for consumption

            return socket_requester_request_wait_for_fulfill(requester, cheri_setbounds(buf, length));
        } else {
            if(length > sock->write_copy_buffer.buffer_size) return E_MSG_SIZE;

            register_t perms = CHERI_PERM_LOAD;
            if(!((flags | sock->flags) & MSG_NO_CAPS)) perms |= CHERI_PERM_LOAD_CAP;

            return socket_internal_data_buffer_to_push_request(buf, length,
                                                               &sock->write_copy_buffer, requester,
                                                               dont_wait, perms);
        }

    } else if(sock->con_type & CONNECT_PULL_WRITE) {
        uni_dir_socket_fulfiller* fulfiller = &sock->write.pull_writer;

        return socket_internal_fulfill(fulfiller, buf, length, sock->flags | flags);
    } else {
        return E_SOCKET_WRONG_TYPE;
    }

}



ssize_t socket_recv(unix_like_socket* sock, char* buf, size_t length, enum SOCKET_FLAGS flags) {

    if(sock->con_type & CONNECT_PULL_READ) {
        uni_dir_socket_requester* requester = sock->read.pull_reader;
        int dont_wait;

        dont_wait = (flags | sock->flags) & MSG_DONT_WAIT;

        if((sock->flags | flags) & MSG_NO_COPY) {
            if(dont_wait) return E_COPY_NEEDED; // We can't not copy and not wait for consumption

            int ret = socket_requester_request_wait_for_fulfill(requester, cheri_setbounds(buf, length));
            if(ret < 0) return ret;
            return length;
        } else {
            // TODO Should I put this in adata  buffer? Probably, or when a flag is given, but
            // TODO need to re-write data buffer management a little bit for that For now this just blocks

            return E_UNSUPPORTED;
        }

    } else if(sock->con_type & CONNECT_PUSH_READ) {
        uni_dir_socket_fulfiller* fulfiller = &sock->read.push_reader;
        return socket_internal_fulfill(fulfiller, buf, length, (flags | sock->flags));
    } else {
        return E_SOCKET_WRONG_TYPE;
    }
}
