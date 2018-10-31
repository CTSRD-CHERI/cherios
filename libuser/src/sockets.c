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

    return 0;
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
                        MAGIC_SAFE
                "li     %[res], 1                   \n"
                "clb    %[res], $zero, 0(%[cc])     \n"
                        MAGIC_SAFE
                "bnez   %[res], 1f                  \n"
                "li     %[res], 2                   \n"
                "clhu   %[res], $zero, 0(%[mc])     \n"
                        MAGIC_SAFE
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

        if(result) syscall_cond_wait(0, 0);

    } while(result);

    return 0;
}

// Until we use exceptions properly this can check whether a requester closed their end even if unmapped
static int socket_internal_fulfiller_closed_safe(uni_dir_socket_fulfiller* fulfiller) {
    volatile uint8_t * req_closed = &fulfiller->requester->requester_closed;
    volatile uint8_t * ful_closed = &fulfiller->requester->fulfiller_component.fulfiller_closed;

    uint8_t rc = 1, fc = 1;
    VMEM_SAFE_DEREFERENCE(req_closed, rc, 8);
    VMEM_SAFE_DEREFERENCE(ful_closed, fc, 8);

    return rc || fc;
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
    uni_dir_socket_requester_fulfiller_component* access = NULL;
    VMEM_SAFE_DEREFERENCE(&fulfiller->requester->access, access, c);
    if(access == NULL) {
        return E_SOCKET_CLOSED;
    }

    if(socket_internal_fulfiller_closed_safe(fulfiller)) {
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

    if(fulfiller->proxy_times != fulfiller->proxy_fin_times) {
        if(dont_wait) return E_IN_PROXY;
        uni_dir_socket_requester* proxying = fulfiller->proxyied_in;
        int ret = socket_internal_sleep_for_condition(&proxying->fulfiller_component.requester_waiting,
                                                   &proxying->fulfiller_component.fulfiller_closed,
                                                   &fulfiller->proxy_fin_times, fulfiller->proxy_times+1, 0xFFFF, delay_sleep);
        return (fulfiller->proxy_times != fulfiller->proxy_fin_times) ? ret : 0; // Ignore errors from the requester if the proxy has finished
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

    requester->requested_bytes += length;
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
    requester->requested_bytes += length;
    return socket_internal_set_and_notify(&requester->requeste_ptr,
                                          request_ptr+1,
                                          &requester->fulfiller_component.fulfiller_waiting);
}

// Requests length bytes to be proxied as fulfillment to fulfiller
ssize_t socket_internal_request_proxy(uni_dir_socket_requester* requester, uni_dir_socket_fulfiller* fulfiller, uint64_t length, uint32_t drb_off) {

    if(fulfiller->proxy_times != fulfiller->proxy_fin_times) {
        if(fulfiller->proxyied_in != requester) return E_IN_PROXY;
    }

    if(fulfiller->socket_type != requester->socket_type) return E_SOCKET_WRONG_TYPE;

    fulfiller->proxyied_in = requester;
    fulfiller->proxy_times +=1;

    uint16_t request_ptr = requester->requeste_ptr;
    uint16_t mask = requester->buffer_size-1;

    request_t* req = &requester->request_ring_buffer[request_ptr & mask];

    req->type = REQUEST_PROXY;
    req->length = length;
    req->request.proxy_for = fulfiller;
    req->drb_fullfill_inc = drb_off;

    requester->requested_bytes += length;
    return socket_internal_set_and_notify(&requester->requeste_ptr,
                                          request_ptr+1,
                                          &requester->fulfiller_component.fulfiller_waiting);
}

ssize_t socket_internal_drb_space_alloc(data_ring_buffer* data_buffer, uint64_t align, uint64_t size, int dont_wait,
                                        char** c1, char**c2, size_t* part1_out, uni_dir_socket_requester* requester) {
    ssize_t res = 0;

    uint64_t extra_to_align = 0;
    uint64_t mask = data_buffer->buffer_size-1;
    uint64_t copy_from = (data_buffer->requeste_ptr);


    uint64_t align_mask = sizeof(capability)-1;
    uint64_t buf_align = (align) & align_mask;
    uint64_t data_buf_align = copy_from & align_mask;


    if(size >= sizeof(capability)) {
        extra_to_align = (buf_align - data_buf_align) & align_mask;
    }

    copy_from = (copy_from + extra_to_align) & mask;
    size_t part_1 = data_buffer->buffer_size - copy_from;
    int two_parts = part_1 < size;

    if(requester) {
        // We will need one or two requests depending if we are wrapping round the buffer
        res = socket_internal_requester_space_wait(requester, two_parts ? 2 : 1, dont_wait, 0);
    }

    if(res < 0) return res;

    uint16_t requeste_ptr;
    uint16_t fulfill_ptr;
    if(requester) {
        requeste_ptr = requester->requeste_ptr;
        fulfill_ptr = requester->fulfiller_component.fulfill_ptr;
    }

    size_t data_space = data_buf_space(data_buffer);
    size += extra_to_align;

    if(data_space < size && (dont_wait || requester == NULL)) return E_AGAIN;

    while(data_space < size) {
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

    // This means the DRB is not being managed properly
    assert_int_ex(data_buf_space(data_buffer), >=, size);
    data_buffer->requeste_ptr += size;
    size -=extra_to_align;

    part_1 = two_parts ? part_1 : size;

    *part1_out = part_1;
    char* cap1 = data_buffer->buffer + copy_from;
    *c1 = cap1;
    char* cap2 = NULL;
    if(two_parts) {
        cap2 = data_buffer->buffer;
    }
    *c2 = cap2;

    return extra_to_align;
}

ssize_t socket_internal_request_ind_db(uni_dir_socket_requester* requester, const char* buf, uint32_t size,
                                       data_ring_buffer* data_buffer,
                                       int dont_wait, register_t perms) {
    ssize_t res;

    if(!data_buffer->buffer) return E_NO_DATA_BUFFER;

    if(size + sizeof(capability) > data_buffer->buffer_size) return E_MSG_SIZE;

    if(size == 0) return 0;

    char* cap1;
    char* cap2;
    size_t part_1;
    res = socket_internal_drb_space_alloc(data_buffer, (uint64_t)buf, size, dont_wait, &cap1, &cap2, &part_1, requester);

    if(res < 0) return res;

    uint64_t align_off = res;

    if(requester->socket_type == SOCK_TYPE_PUSH) {
        memcpy(cap1, buf, part_1);
    }
    cap1 = cheri_andperm(cap1, perms);

    res = socket_internal_request_ind(requester, cap1, part_1, part_1 + align_off);
    if(res < 0) return res;

    if(cap2) {
        size_t part_2 = size - part_1;
        if(requester->socket_type == SOCK_TYPE_PUSH) {
            memcpy(cap2, buf + part_1, part_2);
        }
        cap2 = cheri_andperm(cap2, perms);
        res = socket_internal_request_ind(requester, cap2, part_2, part_2);
        if(res < 0) return res;
        part_1+=part_2;
    }

    assert_int_ex(part_1, ==, size);
    return size;
}

ssize_t socket_internal_request_oob(uni_dir_socket_requester* requester, request_type_e r_type, intptr_t oob_val, uint64_t length, uint32_t drb_off) {
    uint16_t request_ptr = requester->requeste_ptr;
    uint16_t mask = requester->buffer_size-1;

    request_t* req = &requester->request_ring_buffer[request_ptr & mask];

    req->type = r_type;
    req->length = length;
    req->request.oob = oob_val;
    req->drb_fullfill_inc = drb_off;

    requester->requested_bytes += length;
    return socket_internal_set_and_notify(&requester->requeste_ptr,
                                          request_ptr+1,
                                          &requester->fulfiller_component.fulfiller_waiting);
}

void socket_internal_dump_requests(uni_dir_socket_requester* requester) {
    CHERI_PRINT_CAP(requester);
    for(uint16_t i = requester->fulfiller_component.fulfill_ptr; i != requester->requeste_ptr; i++) {
        request_t* req = &requester->request_ring_buffer[i & (requester->buffer_size-1)];
        const char* type_s = (req->type == REQUEST_IM) ? "Immediate" :
                             (req->type == REQUEST_IND ? "Indirect" :
                              (req->type == REQUEST_PROXY) ? "Proxy" :
                              "other");
        if(req->type == REQUEST_IND) {
            CHERI_PRINT_CAP(req->request.ind);
        }

        printf("Type: %10s(%x). Length %8lx. DB_add %8x\n", type_s, req->type, req->length, req->drb_fullfill_inc);
    }
}

void dump_socket(unix_like_socket* sock) {
    if(sock->con_type & CONNECT_PUSH_WRITE) {
        printf("Push writer:\n");
        socket_internal_dump_requests(sock->write.push_writer);
    }
    if(sock->con_type & CONNECT_PUSH_READ) {
        printf("Push read:\n");
        socket_internal_dump_requests(sock->read.push_reader.requester);
    }
    if(sock->con_type & CONNECT_PULL_WRITE) {
        printf("pull writer:\n");
        socket_internal_dump_requests(sock->write.pull_writer.requester);
    }
    if(sock->con_type & CONNECT_PULL_READ) {
        printf("pull read:\n");
        socket_internal_dump_requests(sock->read.pull_reader);
    }
}
// This will fulfill n bytes, progress if progress is set (otherwise this is a peek), and check if check is set.
// Visit will be called on each buffer, with arguments arg, length (for the buffer) and offset + previous lengths
// oob_visit will be called on out of band requests. If oob_visit is null, fulfill stops and E_OOB is returned if
// no fulfillment was made

// If set mark is on (only allowed when peeking) then a mark is placed where progress would otherwise be set to
// When peeking it is allowed to start from the last mark (but not when fulfilling)
ssize_t socket_internal_fulfill_progress_bytes(uni_dir_socket_fulfiller* fulfiller, size_t bytes,
                                               enum FULFILL_FLAGS flags,
                                               ful_func* visit, capability arg, uint64_t offset, ful_oob_func* oob_visit) {

    uni_dir_socket_requester* requester = fulfiller->requester;

    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill %lx bytes. flags %x\n", bytes, flags);

    if((flags & F_PROGRESS) && (flags & (F_START_FROM_LAST_MARK | F_SET_MARK))) {
        if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill bad flags\n");
        return E_BAD_FLAGS;
    }

    if(socket_internal_fulfiller_closed_safe(fulfiller)) {
        return E_SOCKET_CLOSED;
    }

    ssize_t ret;

    // We cannot fulfill anything until proxying is done
    if(!(flags & F_IN_PROXY)) {
        ret = socket_internal_fulfiller_wait_proxy(fulfiller, flags & F_DONT_WAIT, 0);
        if(ret < 0) return ret;
        assert_int_ex(fulfiller->proxy_times, ==, fulfiller->proxy_fin_times);
    }

    uni_dir_socket_requester_fulfiller_component* access = NULL;
    VMEM_SAFE_DEREFERENCE(&fulfiller->requester->access, access, c);
    if(access == NULL) {
        return E_SOCKET_CLOSED;
    }

    size_t bytes_remain = bytes;
    uint64_t partial_bytes = (flags & F_START_FROM_LAST_MARK) ?
                             fulfiller->partial_fulfill_mark_bytes : fulfiller->partial_fulfill_bytes;
    uint16_t mask = requester->buffer_size - 1;
    uint16_t fptr = (flags & F_START_FROM_LAST_MARK) ?
                    fulfiller->fulfill_mark_ptr : requester->fulfiller_component.fulfill_ptr;

    uint16_t required = 1;

    // To account for the fact that we have fast forwarded
    if(flags & F_START_FROM_LAST_MARK)
        required +=
                (fulfiller->fulfill_mark_ptr - fulfiller->requester->fulfiller_component.fulfill_ptr) & mask;

    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill begin %x \n", fptr);
    while(bytes_remain != 0) {

        if((flags & F_CHECK) && partial_bytes == 0) {
             // make sure there is something in the queue to read
            if(flags & F_TRACE) printf("Sock fulfill space wait %x\n", required);
            ret = socket_internal_fulfill_outstanding_wait(fulfiller, required, flags & F_DONT_WAIT, 0);
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
            progress_this = (flags & (F_PROGRESS | F_SET_MARK));
            required++;
        }

        uni_dir_socket_fulfiller* proxy;

        // Bytes to progress can be 0, this might very well be the case for an oob request
        ret = bytes_to_process;

        // Try process this many bytes
        if(req->type & REQUEST_BARRIER) {
            assert(0 && "TODO");
        } else if(req->type == REQUEST_BARRIER_TARGET) {
            assert(0 && "TODO");
            request_t* other_request = req->request.barrier_target;
            volatile act_notify_kt* to_notify = &other_request->request.barrier_waiting;
        }
        else if(req->type == REQUEST_PROXY) {
            if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill proxy\n");
            proxy = req->request.proxy_for;
            ret = socket_internal_fulfill_progress_bytes(proxy, bytes_to_process,
                                                         flags | F_IN_PROXY,
                                                         visit, arg, offset, oob_visit);
        } else {
            if(visit) {
                if (req->type == REQUEST_IM) {
                    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill immediate\n");
                    char *buf = req->request.im + partial_bytes;
                    ret = visit(arg, buf, offset, bytes_to_process);
                } else if (req->type == REQUEST_IND) {
                    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill indirect\n");
                    char *buf = req->request.ind + partial_bytes;
                    ret = visit(arg, buf, offset, bytes_to_process);
                }
            }
            if (req->type >= REQUEST_OUT_BAND) {
                if(oob_visit) {
                    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill Oob visit\n");
                    ret = oob_visit(arg, req, offset, partial_bytes, bytes_to_process);
                } else {
                    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill Oob visit no func\n");
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
            if(progress_this & F_PROGRESS) {
                access->fulfilled_bytes += req->length;
                if(req->type == REQUEST_PROXY) {
                    // If it was a proxy we tell our proxier we are done too
                    uint16_t set_to = proxy->proxy_fin_times+1;
                    uint16_t cmp = proxy->proxy_times;
                    if(set_to == cmp) socket_internal_set_and_notify(&proxy->proxy_fin_times, set_to, &requester->access->requester_waiting);
                    else proxy->proxy_fin_times = set_to;
                }
                if(requester->drb_fulfill_ptr) *requester->drb_fulfill_ptr += req->drb_fullfill_inc;
                socket_internal_set_and_notify(&access->fulfill_ptr, fptr, &access->requester_waiting);
                required = 1;
            } else {
                fulfiller->fulfill_mark_ptr = fptr;
            }
        }

        partial_bytes = new_partial;
        bytes_remain -= bytes_to_process;
        offset += bytes_to_process;
    }

    if(flags & F_PROGRESS) {
        fulfiller->partial_fulfill_bytes = partial_bytes;
    } else if(flags & F_SET_MARK) {
        fulfiller->partial_fulfill_mark_bytes = partial_bytes;
    }

    ssize_t actually_fulfill = bytes - bytes_remain;

    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill finish. %lx bytes fulfilled\n", actually_fulfill);
    return (actually_fulfill == 0) ? ret : actually_fulfill;
}

int socket_internal_fulfiller_reset_check(uni_dir_socket_fulfiller* fulfiller) {
    if(fulfiller->proxy_times != fulfiller->proxy_fin_times) return E_IN_PROXY;
    fulfiller->partial_fulfill_mark_bytes = fulfiller->partial_fulfill_bytes;
    fulfiller->fulfill_mark_ptr = fulfiller->requester->fulfiller_component.fulfill_ptr;
    return 0;
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

uni_dir_socket_requester*  socket_internal_make_read_only(uni_dir_socket_requester* requester) {
    size_t size = SIZE_OF_request(requester->buffer_size);
    requester = cheri_setbounds(requester, size);
    return (uni_dir_socket_requester*)cheri_andperm(requester, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
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
        ro_req = (capability)socket_internal_make_read_only(requester);
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
        ro_req = (capability)socket_internal_make_read_only(requester);
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
    if(fulfiller->connected) return E_ALREADY_CONNECTED;
    fulfiller->requester = requester;
    fulfiller->connected = 1;
    return 0;
}

int socket_internal_requester_connect(uni_dir_socket_requester* requester) {
    if(requester->connected) return E_ALREADY_CONNECTED;
    requester->connected = 1;
    return 0;
}

static int socket_internal_close_safe(volatile uint8_t* own_close, volatile uint8_t* other_close, volatile act_notify_kt * waiter_cap) {
    if(*own_close) return E_ALREADY_CLOSED;

    // We need to signal the other end

    *own_close = 1;

    uint8_t other_close_val = 1;

    VMEM_SAFE_DEREFERENCE(other_close, other_close_val, 8);

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
    uni_dir_socket_requester_fulfiller_component* access = NULL;
    VMEM_SAFE_DEREFERENCE(&fulfiller->requester->access, access, c);
    if(access == NULL) {
        return 0;
    }
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

int init_data_buffer(data_ring_buffer* buffer, char* char_buffer, uint32_t data_buffer_size) {
    if(!is_power_2(data_buffer_size)) return E_BUFFER_SIZE_NOT_POWER_2;

    buffer->buffer_size = data_buffer_size;
    buffer->buffer = cheri_setbounds(char_buffer, data_buffer_size);
    buffer->fulfill_ptr = 0;
    buffer->requeste_ptr = 0;
    buffer->partial_length = 0;
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
        if((con_type & (CONNECT_PUSH_WRITE | CONNECT_PULL_READ)) && (flags & MSG_DONT_WAIT)) return E_SOCKET_WRONG_TYPE;
        if((con_type & CONNECT_PUSH_WRITE) && !(flags & MSG_NO_COPY)) return E_COPY_NEEDED;

        sock->write_copy_buffer.buffer = NULL;
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

    socket_flush_drb(sock);
    if((sock->flags | flags) & MSG_EMULATE_SINGLE_PTR) catch_up_write(sock);

    int dont_wait;
    dont_wait = (flags | sock->flags) & MSG_DONT_WAIT;

    if((sock->flags | flags) & MSG_NO_CAPS) buf = cheri_andperm(buf, CHERI_PERM_LOAD);


    ssize_t ret = E_SOCKET_WRONG_TYPE;
    if(sock->con_type & CONNECT_PUSH_WRITE) {
        uni_dir_socket_requester* requester = sock->write.push_writer;

        if((sock->flags | flags) & MSG_NO_COPY) {
            if(dont_wait) return E_COPY_NEEDED; // We can't not copy and not wait for consumption

            ret = socket_requester_request_wait_for_fulfill(requester, cheri_setbounds(buf, length), length);
            if(ret >= 0) ret = length;

            return length;

        } else {

            register_t perms = CHERI_PERM_LOAD;
            if(!((flags | sock->flags) & MSG_NO_CAPS)) perms |= CHERI_PERM_LOAD_CAP;

            ret = socket_internal_request_ind_db(requester, buf, length, &sock->write_copy_buffer, dont_wait, perms);
        }

    } else if(sock->con_type & CONNECT_PULL_WRITE) {
        uni_dir_socket_fulfiller* fulfiller = &sock->write.pull_writer;
        ful_func * ff = &copy_in;
        enum FULFILL_FLAGS progress = (enum FULFILL_FLAGS)(((sock->flags | flags) & MSG_PEEK) ^ F_PROGRESS);
        ret = socket_internal_fulfill_progress_bytes(fulfiller, length,
                                                      F_CHECK | progress | dont_wait,
                                                      ff, (capability)buf, 0, NULL);
    }

    if(ret > 0 && ((sock->flags | flags) & MSG_EMULATE_SINGLE_PTR)) sock->read_behind+=ret;
    return ret;
}

ssize_t socket_recv(unix_like_socket* sock, char* buf, size_t length, enum SOCKET_FLAGS flags) {

    socket_flush_drb(sock);
    if(((sock->flags | flags) & MSG_EMULATE_SINGLE_PTR)) catch_up_read(sock);

    int dont_wait;
    dont_wait = (flags | sock->flags) & MSG_DONT_WAIT;

    ssize_t ret = E_SOCKET_WRONG_TYPE;
    if(sock->con_type & CONNECT_PULL_READ) {
        uni_dir_socket_requester* requester = sock->read.pull_reader;

        // TODO this use case is confusing. If we put the user buffer in, we could return 0, have async fulfill and then
        // TODO numbers on further reads. However, if the user provided a different buffer on the second call this would
        // TODO break horribly.
        if((sock->flags | flags) & MSG_NO_COPY) {
            if(dont_wait) return E_COPY_NEEDED; // We can't not copy and not wait for consumption

            ret = socket_requester_request_wait_for_fulfill(requester, cheri_setbounds(buf, length), length);
            if(ret >= 0) ret = length;
        } else {
            ret = E_UNSUPPORTED;
        }

    } else if(sock->con_type & CONNECT_PUSH_READ) {
        uni_dir_socket_fulfiller* fulfiller = &sock->read.push_reader;
        ful_func * ff = ((sock->flags | flags) & MSG_NO_CAPS) ? &copy_out_no_caps : copy_out;
        enum FULFILL_FLAGS progress = (enum FULFILL_FLAGS)(((sock->flags | flags) & MSG_PEEK) ^ F_PROGRESS);
        ret = socket_internal_fulfill_progress_bytes(fulfiller, length,
                                                      F_CHECK | progress | dont_wait,
                                                      ff, (capability)buf, 0, NULL);
    }

    if(ret > 0 && ((sock->flags | flags) & MSG_EMULATE_SINGLE_PTR)) sock->write_behind+=ret;

    return ret;
}

ssize_t socket_internal_requester_lseek(uni_dir_socket_requester* requester, int64_t offset, int whence, int dont_wait) {
    ssize_t ret;
    if((ret = socket_internal_requester_space_wait(requester, 1, dont_wait, 0)) != 0) return ret;
    seek_desc desc;
    desc.v.offset = offset;
    desc.v.whence = whence;

    return socket_internal_request_oob(requester, REQUEST_SEEK, desc.as_intptr_t, 0, 0);
}

struct fwf_args {
    uni_dir_socket_fulfiller* writer;
    int dont_wait;
};

static ssize_t socket_internal_fulfill_with_fulfill(capability arg, char* buf, uint64_t offset, uint64_t length) {
    struct fwf_args* args = (struct fwf_args*)arg;
    socket_internal_fulfill_progress_bytes(args->writer, length, F_CHECK | F_PROGRESS | args->dont_wait, &copy_in, (capability)buf, 0, NULL);
}

static ssize_t socket_internal_request_join(uni_dir_socket_requester* pull_req, uni_dir_socket_requester* push_req,
                                            uint64_t align, data_ring_buffer* drb,
                                            size_t count, int dont_wait) {
    if(!drb->buffer) return E_NO_DATA_BUFFER;
    if(dont_wait) return E_UNSUPPORTED;

    assert(push_req->drb_fulfill_ptr != NULL);

    ssize_t res;

    // Chunks of this size on in the steady size (one being written, one being read)
    size_t chunk_size = drb->buffer_size / 2;
    size_t bytes_to_send = count;

    while(bytes_to_send) {
        uint64_t req_size = (chunk_size > bytes_to_send) ? bytes_to_send : chunk_size;

        char* cap1;
        char* cap2;
        size_t part1;
        res = socket_internal_drb_space_alloc(drb, align, req_size, dont_wait, &cap1, &cap2, &part1, push_req);

        if(res < 0) return res;

        uint64_t align_off = res;
        uint64_t size1 = part1;
        uint64_t size2 = req_size - part1;

        socket_internal_request_ind(pull_req, cap1, size1, 0);

        if(cap2) {
            socket_internal_request_ind(pull_req, cap2, size2, 0);
        }

        res = socket_internal_requester_wait_all_finish(pull_req, dont_wait);

        if(res < 0) return res;

        // Make 2 requests

        res = socket_internal_requester_space_wait(push_req, cap2 ? 2 : 1, dont_wait, 0);

        if(res < 0) return res;

        socket_internal_request_ind(push_req, cap1, size1, size1+align_off);
        if(cap2) socket_internal_request_ind(pull_req, cap2, size2, size2);

        bytes_to_send -=req_size;
    }

    return count;
}

ssize_t socket_sendfile(unix_like_socket* sockout, unix_like_socket* sockin, size_t count) {
    uint8_t in_type;
    uint8_t out_type;

    socket_flush_drb(sockout);
    socket_flush_drb(sockin);

    int em_w = sockout->flags & MSG_EMULATE_SINGLE_PTR;
    int em_r = sockin->flags & MSG_EMULATE_SINGLE_PTR;

    if(em_w) catch_up_write(sockout);
    if(em_r) catch_up_read(sockin);

    if(sockin->con_type & CONNECT_PULL_READ) in_type = SOCK_TYPE_PULL;
    else if(sockin->con_type & CONNECT_PUSH_READ) in_type = SOCK_TYPE_PUSH;
    else return E_SOCKET_WRONG_TYPE;

    if(sockout->con_type & CONNECT_PULL_WRITE) out_type = SOCK_TYPE_PULL;
    else if(sockout->con_type & CONNECT_PUSH_WRITE) out_type = SOCK_TYPE_PUSH;
    else return E_SOCKET_WRONG_TYPE;

    int dont_wait = (sockin->flags | sockout->flags) & MSG_DONT_WAIT;
    int no_caps = (sockin->flags | sockout->flags) & MSG_NO_CAPS;

    ssize_t ret;

    if(in_type == SOCK_TYPE_PULL && out_type == SOCK_TYPE_PULL) {
        // Proxy
        uni_dir_socket_requester* pull_read = sockin->read.pull_reader;
        uni_dir_socket_fulfiller* pull_write = &sockout->write.pull_writer;

        ret = socket_internal_request_proxy(pull_read, pull_write, count, 0);
        if(ret >= 0) ret = count;

    } else if(in_type == SOCK_TYPE_PULL && out_type == SOCK_TYPE_PUSH) {
        // Create custom buffer and generate a request (or few) for each
        uni_dir_socket_requester* pull_read = sockin->read.pull_reader;
        uni_dir_socket_requester* push_write = sockout->write.push_writer;

        ret = socket_internal_request_join(pull_read, push_write, 0, &sockout->write_copy_buffer, count, dont_wait);

    } else if(in_type == SOCK_TYPE_PUSH && out_type == SOCK_TYPE_PULL) {
        // fullfill one read with a function that fulfills write
        uni_dir_socket_fulfiller* push_read = &sockin->read.push_reader;
        uni_dir_socket_fulfiller* pull_write = &sockout->write.pull_writer;

        struct fwf_args args;
        args.writer = pull_write;
        args.dont_wait = dont_wait;

        ret = socket_internal_fulfill_progress_bytes(push_read, count, F_CHECK | F_PROGRESS | dont_wait,
                                                      &socket_internal_fulfill_with_fulfill, (capability)&args, 0, NULL);

    } else if(in_type == SOCK_TYPE_PUSH && out_type == SOCK_TYPE_PUSH) {
        // Proxy
        uni_dir_socket_fulfiller* push_read = &sockin->read.push_reader;
        uni_dir_socket_requester* push_write = sockout->write.push_writer;

        socket_internal_request_proxy(push_write, push_read, count, 0);
        if(ret >= 0) ret = count;
    }

    if(ret > 0) {
        if(em_w) sockout->read_behind+=ret;
        if(em_r) sockin->write_behind+=ret;
    }

    return ret;
}

ssize_t socket_flush_drb(unix_like_socket* socket) {
    uint32_t len = socket->write_copy_buffer.partial_length;
    if(socket->write_copy_buffer.buffer && len != 0) {
        if(socket->flags & MSG_EMULATE_SINGLE_PTR) catch_up_write(socket);
        // FIXME respect dont_wait
        // FIXME handle error returns
        char* c1;
        char* c2;
        size_t p1;
        ssize_t align_extra = socket_internal_drb_space_alloc(&socket->write_copy_buffer,
                                        ((size_t)socket->write_copy_buffer.buffer)+socket->write_copy_buffer.requeste_ptr,
                                        len,
                                        0,
                                        &c1,
                                        &c2,
                                        &p1,
                                        socket->write.push_writer);
        assert_int_ex(align_extra, ==, 0);
        socket->write_copy_buffer.partial_length = 0;

        socket_internal_request_ind(socket->write.push_writer, c1, p1, p1);
        if(p1 != len) {
            socket_internal_request_ind(socket->write.push_writer, c2, len - p1, len - p1);
        }
        return len;
    }
    return 0;
}

static void socket_internal_fulfill_cancel_wait(uni_dir_socket_fulfiller* fulfiller) {
    uni_dir_socket_requester_fulfiller_component* access = NULL;
    VMEM_SAFE_DEREFERENCE(&fulfiller->requester->access, access, c);
    if(access == NULL) return;
    fulfiller->requester->access->fulfiller_waiting = NULL;
    if(fulfiller->proxy_times != fulfiller->proxy_fin_times) fulfiller->proxyied_in->fulfiller_component.requester_waiting = NULL;
}

static void socket_internal_request_cancel_wait(uni_dir_socket_requester* requester) {
    requester->fulfiller_component.requester_waiting = NULL;
}

enum poll_events socket_internal_fulfill_poll(uni_dir_socket_fulfiller* fulfiller, enum poll_events io, int set_waiting, int from_check) {
    // Wait until there is something to fulfill and we are not proxying
    enum poll_events ret = POLL_NONE;

    if(!fulfiller->connected) return POLL_NVAL;

    if(io) {

        if(!set_waiting) socket_internal_fulfill_cancel_wait(fulfiller);

        ssize_t wait_res = socket_internal_fulfiller_wait_proxy(fulfiller, !set_waiting, 1);

        if(wait_res == 0) {
            uint16_t amount = 1;
            // We wait for an amount to be present such there is something past our checkpoint
            if(from_check) amount +=
               (fulfiller->fulfill_mark_ptr - fulfiller->requester->fulfiller_component.fulfill_ptr) & (fulfiller->requester->buffer_size-1);
            wait_res = socket_internal_fulfill_outstanding_wait(fulfiller, amount, !set_waiting, 1);

            if(wait_res == 0) ret |= io;
        }

        if(wait_res == E_SOCKET_CLOSED) ret |= POLL_HUP;
        else if(wait_res == E_AGAIN || wait_res == E_IN_PROXY) return ret;
        else if(wait_res < 0) ret |= POLL_ER;

    } else {
        if(socket_internal_fulfiller_closed_safe(fulfiller)) ret |= POLL_HUP;
    }

    return ret;
}

enum poll_events socket_internal_request_poll(uni_dir_socket_requester* requester, enum poll_events io, int set_waiting) {
    // Wait until there is something to request

    enum poll_events ret = POLL_NONE;

    if(!requester->connected) return POLL_NVAL;

    if(io) {
        if(!set_waiting) socket_internal_request_cancel_wait(requester);

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
        ret |= socket_internal_fulfill_poll(pull_write, write, set_waiting, 0);
    } else if(write) return POLL_NVAL;

    if(sock->con_type & CONNECT_PUSH_READ) {
        uni_dir_socket_fulfiller* push_read = &sock->read.push_reader;
        ret |= socket_internal_fulfill_poll(push_read, read, set_waiting, 0);
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
            syscall_cond_wait(msg_queue_poll != 0, 0);
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