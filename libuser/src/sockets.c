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
#include "assert.h"

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

static size_t data_buf_space(data_ring_buffer* data_buffer) {
    return (data_buffer->buffer_size - (data_buffer->requeste_ptr - data_buffer->fulfill_ptr));
}

/* Sets a condition and notifies anybody waiting on it */
static int socket_internal_set_and_notify(volatile uint16_t* ptr, uint16_t new_val, volatile act_kt* waiter_cap) {

    act_kt waiter;

    __asm__ __volatile(
            SANE_ASM
            "cllc   %[res], %[waiting_cap]                     \n"
            "csh    %[new_requeste], $zero, 0(%[new_cap])      \n"
            "cscc   $at, %[res], %[waiting_cap]                \n"
            "clc    %[res], $zero, 0(%[waiting_cap])           \n"
    : [res]"=C"(waiter),
    [new_cap]"+C"(ptr) // This is listed as an output because otherwise it seems to get clobered...
    : [waiting_cap]"C"(waiter_cap),
    [new_requeste]"r"(new_val)
    : "at"
    );

    if(waiter) {
        *waiter_cap = NULL;
        syscall_cond_notify(waiter);
    }
}

/* Checks condition - or sleeps waiting for it */
/* Condition is not((*monitor - im_off) &0xFFFF < comp_val. Also breaks if *closed_cap = 1. This condition can
 * be coerced into doing all the sleeping needs with judicious overflowing */

static int socket_internal_sleep_for_condition(volatile act_kt* wait_cap, volatile uint8_t* closed_cap,
                                               volatile uint16_t* monitor_cap,
                                                uint16_t im_off, uint16_t comp_val) {
    int result;

    do {
        __asm__ __volatile(
                SANE_ASM
                "2: cllc   $c1, %[wc]               \n"
                "clb    %[res], $zero, 0(%[cc])     \n"
                "bnez   %[res], 1f                  \n"
                "li     %[res], 2                   \n"
                "clhu   %[res], $zero, 0(%[mc])     \n"
                "subu   %[res], %[res], %[im]       \n"
                "andi   %[res], %[res], 0xFFFF      \n"
                "sltu   %[res], %[res], %[cmp]      \n"
                "beqz   %[res], 1f                  \n"
                "li     %[res], 0                   \n"
                "cscc   %[res], %[self], %[wc]      \n"
                "beqz   %[res], 2b                  \n"
                "li     %[res], 1                   \n"
                "1:                                 \n"
        : [res]"+r"(result)
        : [wc]"C"(wait_cap), [cc]"C"(closed_cap), [mc]"C"(monitor_cap),[self]"C"(act_self_notify_ref),
                [im]"r"(im_off), [cmp]"r"(comp_val)
        : "$c1"
        );

        if(result == 2) {
            return E_SOCKET_CLOSED;
        }

        if(result) syscall_cond_wait();

    } while(result);

    return 0;
}

int socket_internal_requester_space_wait(uni_dir_socket_requester* requester, uint16_t need_space, int dont_wait) {

    if(requester->fulfiller_component.fulfiller_closed || requester->requester_closed) {
        return E_SOCKET_CLOSED;
    }

    int full = space(requester) < need_space;

    if(!full) return 0;

    if(dont_wait) return E_AGAIN;

    // Funky use of common code, with lots of off by 1 adjustments to be able to use the same comparison
    return socket_internal_sleep_for_condition(&requester->fulfiller_component.requester_waiting,
                                               &requester->fulfiller_component.fulfiller_closed,
                                               &(requester->fulfiller_component.fulfill_ptr),
                                               requester->requeste_ptr+1,
                                               0xFFFF - ((requester->buffer_size - need_space)));
}

// Wait for 'amount' requests to be outstanding
int socket_internal_fulfill_outstanding_wait(uni_dir_socket_fulfiller* fulfiller, uint16_t amount, int dont_wait) {
    uni_dir_socket_requester_fulfiller_component* access = fulfiller->requester->access;

    if(fulfiller->requester->requester_closed || access->fulfiller_closed) {
        return E_SOCKET_CLOSED;
    }

    int empty = fill_level(fulfiller->requester) < amount;

    if(!empty) return 0;

    if(dont_wait) return E_AGAIN;

    return socket_internal_sleep_for_condition(&access->fulfiller_waiting,
                                               &fulfiller->requester->requester_closed,
                                               &(fulfiller->requester->requeste_ptr),
                                               access->fulfill_ptr, amount);
}

// Wait for all requests to be marked as fulfilled
ssize_t socket_internal_requester_wait_all_finish(uni_dir_socket_requester* requester, int dont_wait) {
    return socket_internal_requester_space_wait(requester, requester->buffer_size, dont_wait);
}

// Wait for proxying to be finished
ssize_t socket_internal_fulfiller_wait_proxy(uni_dir_socket_fulfiller* fulfiller, int dont_wait) {

    if(fulfiller->proxy_state) {
        if(dont_wait) return E_IN_PROXY;
        uni_dir_socket_requester* proxying = fulfiller->proxyied_in;
        return socket_internal_sleep_for_condition(&proxying->fulfiller_component.requester_waiting,
                                                   &proxying->fulfiller_component.fulfiller_closed,
                                                   &fulfiller->proxy_state, 1, 1);
    }
    return 0;
}


// NOTE: Before making a request call space_wait for enough space

// Request length (< cap_size) number of bytes. Bytes will be copied in from buf_in, and *buf_out will return the buffer
ssize_t socket_internal_request_im(uni_dir_socket_requester* requester, uint8_t length, char** buf_out, char* buf_in, uint32_t drb_off) {

    if(length > sizeof(capability)) return E_MSG_SIZE;

    uint16_t request_ptr = requester->requeste_ptr;
    uint16_t mask = requester->buffer_size-1;

    request_t* req = &requester->request_ring_buffer[request_ptr & mask];

    req->type = REQUEST_IM;
    req->length = length;
    req->drb_fullfill_inc = drb_off;
    if(buf_in) memcpy(req->request.im, buf_in, length);
    if(buf_out) *buf_out = req->request.im;

    return socket_internal_set_and_notify(&requester->requeste_ptr,
                                          request_ptr+1,
                                          &requester->fulfiller_component.fulfiller_waiting);
}

// Requests length bytes, bytes are put in / taken from buf
ssize_t socket_internal_request_ind(uni_dir_socket_requester* requester, char* buf, uint64_t length, uint32_t drb_off) {

    uint16_t request_ptr = requester->requeste_ptr;
    uint16_t mask = requester->buffer_size-1;

    request_t* req = &requester->request_ring_buffer[request_ptr & mask];

    req->type = REQUEST_IND;
    req->length = length;
    req->request.ind = buf;
    req->drb_fullfill_inc = drb_off;
    return socket_internal_set_and_notify(&requester->requeste_ptr,
                                          request_ptr+1,
                                          &requester->fulfiller_component.fulfiller_waiting);
}

// Requests length bytes to be proxied as fulfillment to fulfiller
ssize_t socket_internal_request_proxy(uni_dir_socket_requester* requester, uni_dir_socket_fulfiller* fulfiller, uint64_t length, uint32_t drb_off) {
    if(fulfiller->proxy_state) return E_IN_PROXY;
    fulfiller->proxyied_in = requester;
    fulfiller->proxy_state = 1;

    uint16_t request_ptr = requester->requeste_ptr;
    uint16_t mask = requester->buffer_size-1;

    request_t* req = &requester->request_ring_buffer[request_ptr & mask];

    req->type = REQUEST_PROXY;
    req->length = length;
    req->request.proxy_for = fulfiller;
    req->drb_fullfill_inc = drb_off;

    return socket_internal_set_and_notify(&requester->requeste_ptr,
                                          request_ptr+1,
                                          &requester->fulfiller_component.fulfiller_waiting);
}

ssize_t socket_internal_request_ind_db(uni_dir_socket_requester* requester, const char* buf, uint32_t size,
                                       data_ring_buffer* data_buffer,
                                       int dont_wait, register_t perms) {
    ssize_t res;

    if(!data_buffer->buffer) return E_NO_DATA_BUFFER;

    if(size + sizeof(capability) > data_buffer->buffer_size) return E_MSG_SIZE;

    if(size == 0) return 0;

    size_t mask = data_buffer->buffer_size-1;

    size_t copy_from = (data_buffer->requeste_ptr);

    size_t extra_to_align = 0;

    // If a buffer is provided we skip a few bytes in the data buffer to match alignment
    if(buf) {
        size_t align_mask = sizeof(capability)-1;
        size_t buf_align = ((size_t)buf) & align_mask;
        size_t data_buf_align = copy_from & align_mask;


        if(size >= sizeof(capability)) {
            extra_to_align = (buf_align - data_buf_align) & align_mask;
        }
    }


    copy_from = (copy_from + extra_to_align) & mask;

    size_t part_1 = data_buffer->buffer_size - copy_from;

    int two_parts = part_1 < size;

    // We will need one or two requests depending if we are wrapping round the buffer
    res = socket_internal_requester_space_wait(requester, two_parts ? 2 : 1, dont_wait);

    if(res < 0 ) return res;

    uint16_t requeste_ptr = requester->requeste_ptr;
    uint16_t fulfill_ptr = requester->fulfiller_component.fulfill_ptr;
    size_t data_space = data_buf_space(data_buffer);

    size += extra_to_align;

    // Can't requeste this message because the data buffer does not have enough space
    if(data_space < size && dont_wait) return E_AGAIN;

    // Reclaim data buffer space until there is enough to copy

    while (data_space < size) {
        uint16_t space = requester->buffer_size - (requeste_ptr - fulfill_ptr);

        if(space == requester->buffer_size) {
            data_space = data_buf_space(data_buffer);
            break;
        }
        // Wait for space to increase by at least 1
        res = socket_internal_requester_space_wait(requester, space+1, dont_wait);

        if(res < 0) return res;

        requester->fulfiller_component.fulfill_ptr;
        data_space = data_buf_space(data_buffer);
    }

    assert(data_space >= size);

    size-=extra_to_align;
    data_buffer->requeste_ptr += extra_to_align;

    // We are okay to make a requeste. First copy to buffer and then send those bytes.
    part_1 = two_parts ? part_1 : size;

    char* cap1 = cheri_setbounds(data_buffer->buffer + copy_from, part_1);

    if(requester->socket_type == SOCK_TYPE_PUSH) {
        memcpy(cap1, buf, part_1);
    }
    cap1 = cheri_andperm(cap1, perms);
    res = socket_internal_request_ind(requester, cap1, part_1, part_1 + extra_to_align);
    if(res < 0) return res;
    data_buffer->requeste_ptr+=part_1;

    if(two_parts) {
        size_t part_2 = size - part_1;
        char* cap2 = cheri_setbounds(data_buffer->buffer, part_2);
        if(requester->socket_type == SOCK_TYPE_PUSH) {
            memcpy(cap2, buf + part_1, part_2);
        }
        cap2 = cheri_andperm(cap2, perms);
        res = socket_internal_request_ind(requester, cap2, part_2, part_2);
        if(res < 0) return res;
        data_buffer->requeste_ptr+=part_2;
    }

    return size;
}

static ssize_t copy_in(capability user_buf, char* req_buf, uint64_t offset, uint64_t length) {
    memcpy(req_buf,(char*)user_buf+offset, length);
    return (ssize_t)length;
}

static ssize_t copy_out(capability user_buf, char* req_buf, uint64_t offset, uint64_t length) {
    memcpy((char*)user_buf+offset, req_buf, length);
    return (ssize_t)length;
}

static ssize_t copy_out_no_caps(capability user_buf, char* req_buf, uint64_t offset, uint64_t length) {
    req_buf = cheri_andperm(req_buf, CHERI_PERM_LOAD);
    memcpy((char*)user_buf+offset, req_buf, length);
    return (ssize_t)length;
}

// This will fulfill n bytes, progress if progress is set (otherwise this is a peek), and check if check is set.
// Visit will be called on each buffer, with arguments arg, length (for the buffer) and offset + previous lengths

ssize_t socket_internal_fulfill_progress_bytes(uni_dir_socket_fulfiller* fulfiller, size_t bytes,
                                               int check, int progress, int dont_wait, int in_proxy,
                                               ful_func* visit, capability arg, uint64_t offset) {

    uni_dir_socket_requester* requester = fulfiller->requester;

    if(requester->requester_closed || requester->fulfiller_component.fulfiller_closed) {
        return E_SOCKET_CLOSED;
    }

    ssize_t ret;

    // We cannot fulfill anything until proxying is done
    if(!in_proxy) {
        ret = socket_internal_fulfiller_wait_proxy(fulfiller, dont_wait);
        if(ret < 0) return ret;
    }

    uni_dir_socket_requester_fulfiller_component* access = fulfiller->requester->access;

    size_t bytes_remain = bytes;
    uint64_t partial_bytes = fulfiller->partial_fulfill_bytes;
    uint16_t mask = requester->buffer_size - 1;
    uint16_t fptr = requester->fulfiller_component.fulfill_ptr;

    uint16_t required = 1;

    while(bytes_remain != 0) {

        if(check && partial_bytes == 0) {
             // make sure there is something in the queue to read
            ret = socket_internal_fulfill_outstanding_wait(fulfiller, required, dont_wait);
            if(ret < 0) break;
        }

        request_t* req = &requester->request_ring_buffer[(fptr)&mask];

        // Work out how much we should process

        uint64_t effective_len = req->length - partial_bytes;

        uint64_t new_partial;
        uint64_t bytes_to_process;
        int progress_this;
        if(effective_len > bytes_remain) {
            // We don't free this one
            new_partial = partial_bytes + bytes_remain;
            bytes_to_process = bytes_remain;
            progress_this = 0;
        } else {
            // We free this one
            new_partial = 0;
            bytes_to_process = effective_len;
            fptr++;
            progress_this = progress;
            required++;
        }

        uni_dir_socket_fulfiller* proxy;

        ret = bytes_to_process;

        assert(bytes_to_process != 0);
        // Try process this many bytes
        if(req->type == REQUEST_PROXY) {
            proxy = req->request.proxy_for;
            ret = socket_internal_fulfill_progress_bytes(proxy, bytes_to_process,
                                                         check, progress, dont_wait, 1,
                                                         visit, arg, offset);
        } else if(visit) {
            if(req->type == REQUEST_IM) {
                char* buf = req->request.im + partial_bytes;
                ret = visit(arg, buf, offset, bytes_to_process);
            } else if(req->type == REQUEST_IND) {
                char* buf = req->request.ind + partial_bytes;
                ret = visit(arg, buf, offset, bytes_to_process);
            }
        }

        // We may fail for some reason, in which case ret is an error, or how many bytes we actually managed
        if(ret != bytes_to_process) {
            if(ret > 0) {
                partial_bytes += ret;
                bytes_remain -= ret;
            }
            break;
        }

        // Release this request as it is finished
        if(progress_this) {
            if(req->type == REQUEST_PROXY) {
                // If it was a proxy we tell our proxier we are done too
                socket_internal_set_and_notify(&proxy->proxy_state, 0, &requester->access->requester_waiting);
            }
            if(requester->drb_fulfill_ptr) *requester->drb_fulfill_ptr += req->drb_fullfill_inc;
            socket_internal_set_and_notify(&access->fulfill_ptr, fptr, &access->requester_waiting);
            required = 1;
        }

        partial_bytes = new_partial;
        bytes_remain -= bytes_to_process;
        offset += bytes_to_process;
    }

    if(progress) {
        fulfiller->partial_fulfill_bytes = partial_bytes;
    }

    ssize_t actually_fulfill = bytes - bytes_remain;

    return (actually_fulfill == 0) ? ret : actually_fulfill;
}

int socket_internal_fulfiller_init(uni_dir_socket_fulfiller* fulfiller, uint8_t socket_type) {
    fulfiller->requester = NULL;
    fulfiller->partial_fulfill_bytes = 0;
    fulfiller->socket_type = socket_type;
    return 0;
}

int socket_internal_requester_init(uni_dir_socket_requester* requester, uint16_t buffer_size, uint8_t socket_type, data_ring_buffer* paired_drb) {
    if(!is_power_2(buffer_size)) return E_BUFFER_SIZE_NOT_POWER_2;

    bzero(requester, SIZE_OF_request(buffer_size));
    requester->buffer_size = buffer_size;
    requester->socket_type = socket_type;
    if(paired_drb) {
        requester->drb_fulfill_ptr = &paired_drb->fulfill_ptr;
    }
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


static int init_data_buffer(data_ring_buffer* buffer, char* char_buffer, uint32_t data_buffer_size) {
    if(!is_power_2(data_buffer_size)) return E_BUFFER_SIZE_NOT_POWER_2;

    buffer->buffer_size = data_buffer_size;
    buffer->buffer = cheri_setbounds(char_buffer, data_buffer_size);
    buffer->fulfill_ptr = 0;
    buffer->requeste_ptr = 0;

    return 0;
}

int socket_init(unix_like_socket* sock, enum SOCKET_FLAGS flags,
                char* data_buffer, uint32_t data_buffer_size,
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

ssize_t socket_requester_request_wait_for_fulfill(uni_dir_socket_requester* requester, char* buf, uint64_t length) {
    int ret;

    ret = socket_internal_requester_space_wait(requester, 1, 0);

    if(ret < 0) return ret;

    socket_internal_request_ind(requester, buf, length, 0);

    // Wait for everything to be consumed
    return socket_internal_requester_wait_all_finish(requester, 0);
}

ssize_t socket_send(unix_like_socket* sock, const char* buf, size_t length, enum SOCKET_FLAGS flags) {
    // Need to copy to buffer in order not to expose buf

    int dont_wait;
    dont_wait = (flags | sock->flags) & MSG_DONT_WAIT;

    if((sock->flags | flags) & MSG_NO_CAPS) buf = cheri_andperm(buf, CHERI_PERM_LOAD);

    if(sock->con_type & CONNECT_PUSH_WRITE) {
        uni_dir_socket_requester* requester = sock->write.push_writer;

        if((sock->flags | flags) & MSG_NO_COPY) {
            if(dont_wait) return E_COPY_NEEDED; // We can't not copy and not wait for consumption

            return socket_requester_request_wait_for_fulfill(requester, cheri_setbounds(buf, length), length);
        } else {

            register_t perms = CHERI_PERM_LOAD;
            if(!((flags | sock->flags) & MSG_NO_CAPS)) perms |= CHERI_PERM_LOAD_CAP;

            return socket_internal_request_ind_db(requester, buf, length, &sock->write_copy_buffer, dont_wait, perms);;
        }

    } else if(sock->con_type & CONNECT_PULL_WRITE) {
        uni_dir_socket_fulfiller* fulfiller = &sock->write.pull_writer;
        ful_func * ff = &copy_in;
        return socket_internal_fulfill_progress_bytes(fulfiller, length,
                                                      1, ~((sock->flags | flags) & MSG_PEEK), dont_wait, 0,
                                                      ff, (capability)buf, 0);
    } else {
        return E_SOCKET_WRONG_TYPE;
    }

}

ssize_t socket_recv(unix_like_socket* sock, char* buf, size_t length, enum SOCKET_FLAGS flags) {
    int dont_wait;
    dont_wait = (flags | sock->flags) & MSG_DONT_WAIT;

    if(sock->con_type & CONNECT_PULL_READ) {
        uni_dir_socket_requester* requester = sock->read.pull_reader;

        // TODO this use case is confusing. If we put the user buffer in, we could return 0, have async fulfill and then
        // TODO numbers on further reads. However, if the user provided a different buffer on the second call this would
        // TODO break horribly.
        if((sock->flags | flags) & MSG_NO_COPY) {
            if(dont_wait) return E_COPY_NEEDED; // We can't not copy and not wait for consumption

            int ret = socket_requester_request_wait_for_fulfill(requester, cheri_setbounds(buf, length), length);
            if(ret < 0) return ret;
            return length;
        } else {

            return E_UNSUPPORTED;
        }

    } else if(sock->con_type & CONNECT_PUSH_READ) {
        uni_dir_socket_fulfiller* fulfiller = &sock->read.push_reader;
        ful_func * ff = ((sock->flags | flags) & MSG_NO_CAPS) ? &copy_out_no_caps : copy_out;
        return socket_internal_fulfill_progress_bytes(fulfiller, length,
                                                      1, ~((sock->flags | flags) & MSG_PEEK), dont_wait, 0,
                                                      ff, (capability)buf, 0);
    } else {
        return E_SOCKET_WRONG_TYPE;
    }
}
