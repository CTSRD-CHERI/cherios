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

#include <sockets.h>
#include <queue.h>
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
                                                uint16_t im_off, uint16_t comp_val, int delay_sleep) {
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

        if(delay_sleep) return result;

        if(result) syscall_cond_wait(0);

    } while(result);

    return 0;
}

int socket_internal_requester_space_wait(uni_dir_socket_requester* requester, uint16_t need_space, int dont_wait, int delay_sleep) {

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
                                               0xFFFF - ((requester->buffer_size - need_space)), delay_sleep);
}

// Wait for 'amount' requests to be outstanding
int socket_internal_fulfill_outstanding_wait(uni_dir_socket_fulfiller* fulfiller, uint16_t amount, int dont_wait, int delay_sleep) {
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
                                               access->fulfill_ptr, amount, delay_sleep);
}

// Wait for all requests to be marked as fulfilled
ssize_t socket_internal_requester_wait_all_finish(uni_dir_socket_requester* requester, int dont_wait) {
    ssize_t ret = socket_internal_requester_space_wait(requester, requester->buffer_size, dont_wait, 0);
    if(ret != 0 && fill_level(requester) == 0) ret = 0;
    return ret;
}

// Wait for proxying to be finished
ssize_t socket_internal_fulfiller_wait_proxy(uni_dir_socket_fulfiller* fulfiller, int dont_wait, int delay_sleep) {

    if(fulfiller->proxy_state) {
        if(dont_wait) return E_IN_PROXY;
        uni_dir_socket_requester* proxying = fulfiller->proxyied_in;
        int ret = socket_internal_sleep_for_condition(&proxying->fulfiller_component.requester_waiting,
                                                   &proxying->fulfiller_component.fulfiller_closed,
                                                   &fulfiller->proxy_state, 1, 1, delay_sleep);
        return (fulfiller->proxy_state) ? ret : 0; // Ignore errors from the requester if the proxy has finished
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
    if(fulfiller->socket_type != requester->socket_type) return E_SOCKET_WRONG_TYPE;

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
    res = socket_internal_requester_space_wait(requester, two_parts ? 2 : 1, dont_wait, 0);

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
        res = socket_internal_requester_space_wait(requester, space+1, dont_wait, 0);

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

void socket_internal_dump_requests(uni_dir_socket_requester* requester) {
    for(uint16_t i = requester->fulfiller_component.fulfill_ptr; i != requester->requeste_ptr; i++) {
        request_t* req = &requester->request_ring_buffer[i & (requester->buffer_size-1)];
        const char* type_s = (req->type == REQUEST_IM) ? "Immediate" :
                             (req->type == REQUEST_IND ? "Indirect" :
                              (req->type == REQUEST_PROXY) ? "Proxy" :
                              "other");
        if(req->type == REQUEST_IND) {
            CHERI_PRINT_CAP(req->request.ind);
        }

        printf("Type: %10s. Length %8lx. DB_add %8x\n", type_s, req->length, req->drb_fullfill_inc);
    }
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
// oob_visit will be called on out of band requests. If oob_visit is null, fulfill stops and E_OOB is returned if
// no fulfillment was made

ssize_t socket_internal_fulfill_progress_bytes(uni_dir_socket_fulfiller* fulfiller, size_t bytes,
                                               int check, int progress, int dont_wait, int in_proxy,
                                               ful_func* visit, capability arg, uint64_t offset, ful_oob_func* oob_visit) {

    uni_dir_socket_requester* requester = fulfiller->requester;

    if(requester->requester_closed || requester->fulfiller_component.fulfiller_closed) {
        return E_SOCKET_CLOSED;
    }

    ssize_t ret;

    // We cannot fulfill anything until proxying is done
    if(!in_proxy) {
        ret = socket_internal_fulfiller_wait_proxy(fulfiller, dont_wait, 0);
        if(ret < 0) return ret;
        assert_int_ex(fulfiller->proxy_state, ==, 0);
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
            ret = socket_internal_fulfill_outstanding_wait(fulfiller, required, dont_wait, 0);
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

        // Bytes to progress can be 0, this might very well be the case for an oob request
        ret = bytes_to_process;

        // Try process this many bytes
        if(req->type == REQUEST_PROXY) {
            proxy = req->request.proxy_for;
            ret = socket_internal_fulfill_progress_bytes(proxy, bytes_to_process,
                                                         check, progress, dont_wait, 1,
                                                         visit, arg, offset, oob_visit);
        } else {
            if(visit) {
                if (req->type == REQUEST_IM) {
                    char *buf = req->request.im + partial_bytes;
                    ret = visit(arg, buf, offset, bytes_to_process);
                } else if (req->type == REQUEST_IND) {
                    char *buf = req->request.ind + partial_bytes;
                    ret = visit(arg, buf, offset, bytes_to_process);
                }
            }
            if (req->type == REQUEST_OUT_BAND) {
                if(oob_visit) {
                    ret = oob_visit(req, offset, partial_bytes, bytes_to_process);
                } else {
                    ret = E_OOB;
                }
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
    bzero(fulfiller, sizeof(uni_dir_socket_fulfiller));
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
        if(requester->connected) return E_ALREADY_CONNECTED;
        ro_req = (capability)cheri_andperm(requester, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
        if(requester->socket_type == SOCK_TYPE_PULL) {
            con_type |= CONNECT_PULL_WRITE;
        } else {
            con_type |= CONNECT_PUSH_READ;
        }
    }

    if(fulfiller) {
        if(fulfiller->connected) return E_ALREADY_CONNECTED;
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

    if(requester) requester->connected = 1;
    if(fulfiller) fulfiller->connected = 1;
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
        fulfiller->connected = 1;
    }

    if(requester) requester->connected = 1;

    return 0;
}

int socket_internal_fulfiller_connect(uni_dir_socket_fulfiller* fulfiller, uni_dir_socket_requester* requester) {
    fulfiller->requester = requester;
    fulfiller->connected = 1;
}

int socket_internal_requester_connect(uni_dir_socket_requester* requester) {
    requester->connected = 1;
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

ssize_t socket_internal_close_requester(uni_dir_socket_requester* requester, int wait_finish, int dont_wait) {
    if(wait_finish) {
        ssize_t ret = socket_internal_requester_wait_all_finish(requester, dont_wait);
        // Its O.K for the other end to close if all requests are done
        if(ret < 0) return ret;
    }
    return socket_internal_close_safe(&requester->requester_closed,
                                      &requester->fulfiller_component.fulfiller_closed,
                                      &requester->fulfiller_component.fulfiller_waiting);
}

ssize_t socket_internal_close_fulfiller(uni_dir_socket_fulfiller* fulfiller, int wait_finish, int dont_wait) {
    uni_dir_socket_requester_fulfiller_component* access = fulfiller->requester->access;
    if(wait_finish) {
        ssize_t ret = socket_internal_fulfiller_wait_proxy(fulfiller, dont_wait, 0);
        if(ret < 0) return ret;
    }
    return socket_internal_close_safe(&access->fulfiller_closed,
                                      &fulfiller->requester->requester_closed,
                                      &access->requester_waiting);
}

ssize_t socket_close(unix_like_socket* sock) {
    int dont_wait = sock->flags & MSG_DONT_WAIT;
    ssize_t ret;

    if(sock->con_type & CONNECT_PULL_READ) {
        ret = socket_internal_close_requester(sock->read.pull_reader, 1, dont_wait);
    } else if(sock->con_type & CONNECT_PUSH_READ) {
        ret = socket_internal_close_fulfiller(&sock->read.push_reader, 1, dont_wait);
    }

    if(ret < 0) return  ret;

    if(sock->con_type & CONNECT_PUSH_WRITE) {
        ret = socket_internal_close_requester(sock->write.push_writer, 1, dont_wait);
    } else if(sock->con_type & CONNECT_PULL_WRITE) {
        ret = socket_internal_close_fulfiller(&sock->write.pull_writer, 1, dont_wait);
    }

    return ret;
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

    ret = socket_internal_requester_space_wait(requester, 1, 0, 0);

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

            ssize_t ret = socket_requester_request_wait_for_fulfill(requester, cheri_setbounds(buf, length), length);
            if(ret < 0) return  ret;
            return length;

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
                                                      ff, (capability)buf, 0, NULL);
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
                                                      ff, (capability)buf, 0, NULL);
    } else {
        return E_SOCKET_WRONG_TYPE;
    }
}

struct fwf_args {
    uni_dir_socket_fulfiller* writer;
    int dont_wait;
};

static ssize_t socket_internal_fulfill_with_fulfill(capability arg, char* buf, uint64_t offset, uint64_t length) {
    struct fwf_args* args = (struct fwf_args*)arg;
    socket_internal_fulfill_progress_bytes(args->writer, length, 1, 1, args->dont_wait, 0, &copy_in, (capability)buf, 0, NULL);
}

static ssize_t socket_internal_request_join(uni_dir_socket_requester* pull_req, uni_dir_socket_requester* push_req,
                                            uint64_t align, data_ring_buffer* drb,
                                            int dont_wait) {
    return E_UNSUPPORTED;
}

ssize_t socket_sendfile(unix_like_socket* sockout, unix_like_socket* sockin, size_t count) {
    uint8_t in_type;
    uint8_t out_type;

    if(sockin->con_type & CONNECT_PULL_READ) in_type = SOCK_TYPE_PULL;
    else if(sockin->con_type & CONNECT_PUSH_READ) in_type = SOCK_TYPE_PUSH;
    else return E_SOCKET_WRONG_TYPE;

    if(sockout->con_type & CONNECT_PULL_WRITE) out_type = SOCK_TYPE_PULL;
    else if(sockout->con_type & CONNECT_PUSH_WRITE) out_type = SOCK_TYPE_PUSH;
    else return E_SOCKET_WRONG_TYPE;

    int dont_wait = (sockin->flags | sockout->flags) & MSG_DONT_WAIT;
    int no_caps = (sockin->flags | sockout->flags) & MSG_NO_CAPS;

    if(in_type == SOCK_TYPE_PULL && out_type == SOCK_TYPE_PULL) {
        // Proxy
        uni_dir_socket_requester* pull_read = sockin->read.pull_reader;
        uni_dir_socket_fulfiller* pull_write = &sockout->write.pull_writer;

        ssize_t ret = socket_internal_request_proxy(pull_read, pull_write, count, 0);
        return (ret < 0) ? ret : count;

    } else if(in_type == SOCK_TYPE_PULL && out_type == SOCK_TYPE_PUSH) {
        // Create custom buffer and generate a request (or few) for each
        uni_dir_socket_requester* pull_read = sockin->read.pull_reader;
        uni_dir_socket_requester* push_write = sockout->write.push_writer;

        return socket_internal_request_join(pull_read, push_write, 0, &sockout->write_copy_buffer, dont_wait);

    } else if(in_type == SOCK_TYPE_PUSH && out_type == SOCK_TYPE_PULL) {
        // fullfill one read with a function that fulfills write
        uni_dir_socket_fulfiller* push_read = &sockin->read.push_reader;
        uni_dir_socket_fulfiller* pull_write = &sockout->write.pull_writer;

        struct fwf_args args;
        args.writer = pull_write;
        args.dont_wait = dont_wait;

        return socket_internal_fulfill_progress_bytes(push_read, count, 1, 1, dont_wait, 0,
                                                      &socket_internal_fulfill_with_fulfill, (capability)&args, 0, NULL);

    } else if(in_type == SOCK_TYPE_PUSH && out_type == SOCK_TYPE_PUSH) {
        // Proxy
        uni_dir_socket_fulfiller* push_read = &sockin->read.push_reader;
        uni_dir_socket_requester* push_write = sockout->write.push_writer;

        ssize_t ret = socket_internal_request_proxy(push_write, push_read, count, 0);
        return (ret < 0) ? ret : count;
    }
}

static void socket_internal_fulfill_cancel_wait(uni_dir_socket_fulfiller* fulfiller) {
    fulfiller->requester->access->fulfiller_waiting = NULL;
    if(fulfiller->proxy_state) fulfiller->proxyied_in->fulfiller_component.requester_waiting = NULL;
}

static void socket_internal_request_cancel_wait(uni_dir_socket_requester* requester) {
    requester->fulfiller_component.requester_waiting = NULL;
}

static enum poll_events socket_internal_fulfill_poll(uni_dir_socket_fulfiller* fulfiller, enum poll_events io, int set_waiting) {
    // Wait until there is something to fulfill and we are not proxying
    enum poll_events ret = POLL_NONE;

    if(!fulfiller->connected) return POLL_NVAL;

    if(io) {

        if(!set_waiting) socket_internal_fulfill_cancel_wait(fulfiller);

        ssize_t wait_res = socket_internal_fulfiller_wait_proxy(fulfiller, !set_waiting, 1);

        if(wait_res == 0) {
            wait_res = socket_internal_fulfill_outstanding_wait(fulfiller, 1, !set_waiting, 1);

            if(wait_res == 0) ret |= io;
        }

        if(wait_res == E_SOCKET_CLOSED) ret |= POLL_HUP;
        else if(wait_res == E_AGAIN || wait_res == E_IN_PROXY) return ret;
        else if(wait_res < 0) ret |= POLL_ER;

    } else {
        if(fulfiller->requester->requester_closed ||
           fulfiller->requester->fulfiller_component.fulfiller_closed) ret |= POLL_HUP;
    }

    return ret;
}

static enum poll_events socket_internal_request_poll(uni_dir_socket_requester* requester, enum poll_events io, int set_waiting) {
    // Wait until there is something to request

    enum poll_events ret = POLL_NONE;

    if(!requester->connected) return POLL_NVAL;

    if(io) {
        int wait_res = socket_internal_requester_space_wait(requester, 1, !set_waiting, 1);

        if(wait_res == 0) ret |= io;
        else if(wait_res == E_AGAIN) return ret;
        else if(wait_res < 0) ret |= POLL_ER;
    } else {
        if(requester->requester_closed || requester->fulfiller_component.fulfiller_closed) ret |= POLL_HUP;
    }

    return ret;
}

static enum poll_events be_waiting_for_event(unix_like_socket* sock, enum poll_events asked_events, int set_waiting) {

    enum poll_events read = asked_events & POLL_IN;
    enum poll_events write = asked_events & POLL_OUT;

    enum poll_events ret = POLL_NONE;


    if(sock->con_type & CONNECT_PUSH_WRITE) {
        uni_dir_socket_requester* push_write = sock->write.push_writer;
        ret |= socket_internal_request_poll(push_write, write, set_waiting);
    } else if(sock->con_type & CONNECT_PULL_WRITE) {
        uni_dir_socket_fulfiller* pull_write = &sock->write.pull_writer;
        ret |= socket_internal_fulfill_poll(pull_write, write, set_waiting);
    } else if(write) return POLL_NVAL;

    if(sock->con_type & CONNECT_PUSH_READ) {
        uni_dir_socket_fulfiller* push_read = &sock->read.push_reader;
        ret |= socket_internal_fulfill_poll(push_read, read, set_waiting);
    } else if(sock->con_type & CONNECT_PULL_READ) {
        uni_dir_socket_requester* pull_read = sock->read.pull_reader;
        ret |= socket_internal_request_poll(pull_read, read, set_waiting);
    } else if(read) return POLL_NVAL;

    return ret;
}

static int sockets_scan(poll_sock_t* socks, size_t nsocks, enum poll_events* msg_queue_poll, int sleep) {

    int any_event = 0;

    enum poll_events events_forced = POLL_ER | POLL_HUP | POLL_NVAL;

    if(sleep) {
        // This is an optimisation. Race conditions on setting multiple waiters may result in a stampede of notifies.
        // We don't cancel after this happens (the last poll) as probably also misses the race. We cancel much later
        // At the usage of the next sleep.
        syscall_cond_cancel();
    }

    restart:
    do {
        if(msg_queue_poll) {
            *msg_queue_poll = POLL_NONE;
            if(!msg_queue_empty()) {
                *msg_queue_poll = POLL_IN;
                sleep = 0;
                any_event++;
            }
        }

        for(size_t i = 0; i != nsocks; i++) {
            poll_sock_t* sock_poll = socks+i;
            enum poll_events asked_events = (events_forced | sock_poll->events);

            enum poll_events revents = be_waiting_for_event(sock_poll->sock, asked_events, sleep);

            if(asked_events & revents) {

                if(sleep) {
                    sleep = 0;
                    goto restart;
                }
                any_event++;
            }

            sock_poll->revents = revents;
        }

        if(sleep) {
            syscall_cond_wait(msg_queue_poll != 0);
        }

    } while(sleep);

    return any_event;
}

int socket_poll(poll_sock_t* socks, size_t nsocks, enum poll_events* msg_queue_poll) {
    int ret;

    if(nsocks < 0) return 0;

    if(nsocks == 0 && !msg_queue_poll) return 0;

    if(!(ret = sockets_scan(socks, nsocks, msg_queue_poll, 0))) {
        ret = sockets_scan(socks, nsocks, msg_queue_poll, 1);
    }

    return ret;
}