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

#include "libsockets.h"
#include <queue.h>
#include "object.h"
#include "string.h"
#include "nano/nanokernel.h"
#include "assert.h"
#include "cheric.h"

//#define printf(...) syscall_printf(__VA_ARGS__)
#define printf(...) // Printf now not in the same dynamic library. To get this back finish dynamic linking.


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
            "cscc   $at, %[res], %[waiting_cap]                \n" // FIXME: Might be wrong. We might fail to fail the other process as our own store fails
            "clc    %[res], $zero, 0(%[waiting_cap])           \n"
    : [res]"=&C"(waiter)
    : [waiting_cap]"C"(waiter_cap), [new_cap]"C"(ptr), [new_requeste]"r"(new_val)
    : "at", "memory"
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
        : [res]"=&r"(result)
        : [wc]"C"(wait_cap), [cc]"C"(closed_cap), [mc]"C"(monitor_cap),[self]"C"(act_self_notify_ref),
                [im]"r"(im_off), [cmp]"r"(comp_val)
        : "$c1", "memory"
        );

        if(result == 2) {
            *wait_cap = NULL;
            return E_SOCKET_CLOSED;
        }

        if(delay_sleep) return result;

        if(result) syscall_cond_wait(0, 0);

    } while(result);

    *wait_cap = NULL;
    return 0;
}

// Until we use exceptions properly this can check whether a requester closed their end even if unmapped
static int socket_internal_fulfiller_closed_safe(uni_dir_socket_fulfiller* fulfiller) {
    if(!fulfiller->connected) return 0;
    volatile uint8_t * req_closed = &fulfiller->requester->requester_closed;
    volatile uint8_t * ful_closed = &fulfiller->requester->fulfiller_component.fulfiller_closed;

    uint8_t rc = 1, fc = 1;
    VMEM_SAFE_DEREFERENCE(req_closed, rc, 8);
    VMEM_SAFE_DEREFERENCE(ful_closed, fc, 8);

    return rc || fc;
}

#define TRUNCATE16(X) ({uint16_t _tmpx; __asm ("andi   %[out], %[in], 0xFFFF\n":[out]"=r"(_tmpx):[in]"r"(X):); _tmpx;})

static int socket_internal_requester_space_wait(uni_dir_socket_requester* requester, uint16_t need_space, int dont_wait, int delay_sleep) {

    if(requester->fulfiller_component.fulfiller_closed || requester->requester_closed) {
        return E_SOCKET_CLOSED;
    }

    if(need_space == SPACE_AMOUNT_ALL) need_space = requester->buffer_size;

    // FIXME: We really should check this here unless this is being called from fulfill.
    // FIXME: Currently can't be bothered with the refactor
    // if(requester->joined) return E_IN_JOIN;

    int full = space(requester) < need_space;

    if(!full) return 0;

    if(dont_wait) return E_AGAIN;

    uint16_t amount = ((~(requester->buffer_size - need_space)));
    amount = TRUNCATE16(amount);


    // Funky use of common code, with lots of off by 1 adjustments to be able to use the same comparison
    return socket_internal_sleep_for_condition(&requester->fulfiller_component.requester_waiting,
                                               &requester->fulfiller_component.fulfiller_closed,
                                               &(requester->fulfiller_component.fulfill_ptr),
                                               requester->requeste_ptr+1,
                                               amount, delay_sleep);
}

int socket_requester_space_wait(requester_t requester, uint16_t need_space, int dont_wait, int delay_sleep) {
    uni_dir_socket_requester* r = UNSEAL_CHECK_REQUESTER(requester);
    if(!r) return E_BAD_SEAL;
    return socket_internal_requester_space_wait(r, need_space, dont_wait, delay_sleep);
}

// Wait for 'amount' requests to be outstanding
static int socket_internal_fulfill_outstanding_wait(uni_dir_socket_fulfiller* fulfiller, uint16_t amount, int dont_wait, int delay_sleep) {
    uni_dir_socket_requester_fulfiller_component* access = NULL;
    VMEM_SAFE_DEREFERENCE(&fulfiller->requester->access, access, c);
    if(access == NULL) {
        return E_SOCKET_CLOSED;
    }

    if(socket_internal_fulfiller_closed_safe(fulfiller)) {
        return E_SOCKET_CLOSED;
    }

    if(amount == SPACE_AMOUNT_ALL) amount = fulfiller->requester->buffer_size;

    int empty = fill_level(fulfiller->requester) < amount;

    if(!empty) return 0;

    if(dont_wait) return E_AGAIN;

    return socket_internal_sleep_for_condition(&access->fulfiller_waiting,
                                               &fulfiller->requester->requester_closed,
                                               &(fulfiller->requester->requeste_ptr),
                                               access->fulfill_ptr, amount, delay_sleep);
}

int socket_fulfiller_outstanding_wait(fulfiller_t fulfiller, uint16_t amount, int dont_wait, int delay_sleep) {
    uni_dir_socket_requester* f = UNSEAL_CHECK_REQUESTER(fulfiller);
    if(!f) return E_BAD_SEAL;
    return socket_internal_fulfill_outstanding_wait(f, amount, dont_wait, delay_sleep);
}

// Wait for all requests to be marked as fulfilled
ssize_t socket_requester_wait_all_finish(requester_t * r, int dont_wait) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    if(!requester) return E_BAD_SEAL;
    ssize_t ret = socket_internal_requester_space_wait(requester, requester->buffer_size, dont_wait, 0);
    if(ret != 0 && fill_level(requester) == 0) ret = 0;
    return ret;
}

// Wait for proxying to be finished
static ssize_t socket_internal_fulfiller_wait_proxy(uni_dir_socket_fulfiller* fulfiller, int dont_wait, int delay_sleep) {

    if(fulfiller->proxy_times != fulfiller->proxy_fin_times) {
        if(dont_wait) return E_IN_PROXY;
        uni_dir_socket_requester* proxying = fulfiller->proxyied_in;
        int ret = socket_internal_sleep_for_condition(&proxying->fulfiller_component.requester_waiting,
                                                   &proxying->fulfiller_component.fulfiller_closed,
                                                   &fulfiller->proxy_fin_times, fulfiller->proxy_times+1, 0xFFFFu, delay_sleep);
        return (fulfiller->proxy_times != fulfiller->proxy_fin_times) ? ret : 0; // Ignore errors from the requester if the proxy has finished
    }
    return 0;
}


// NOTE: Before making a request call space_wait for enough space

// Request length (< cap_size) number of bytes. Bytes will be copied in from buf_in, and *buf_out will return the buffer
ssize_t socket_request_im(requester_t r, uint8_t length, char** buf_out, char* buf_in, uint32_t drb_off) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    if(!requester) return E_BAD_SEAL;


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
static ssize_t socket_internal_request_ind(uni_dir_socket_requester* requester, char* buf, uint64_t length, uint32_t drb_off) {

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

// Requests length bytes, bytes are put in / taken from buf
ssize_t socket_request_ind(requester_t r, char* buf, uint64_t length, uint32_t drb_off) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    if(!requester) return E_BAD_SEAL;

    return socket_internal_request_ind(requester, buf, length, drb_off);
}

// Requests length bytes to be proxied as fulfillment to fulfiller
ssize_t socket_request_proxy(requester_t r, fulfiller_t f, uint64_t length, uint32_t drb_off) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    uni_dir_socket_fulfiller* fulfiller = UNSEAL_CHECK_FULFILLER(f);

    if(!requester || !fulfiller) return E_BAD_SEAL;

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

// Requests length bytes be fulfilled by a puller, and then pushed into a push request
// A drb is provided, but either party may substitute their own (TODO: Only the writer currently can do this)
ssize_t socket_request_join(requester_t pull_req, requester_t push_req, data_ring_buffer* drb, uint64_t length, uint32_t drb_off) {

    uni_dir_socket_requester* pull = UNSEAL_CHECK_REQUESTER(pull_req);
    uni_dir_socket_requester* push = UNSEAL_CHECK_REQUESTER(push_req);

    if(!pull || !push) return E_BAD_SEAL;

    if(pull->socket_type != SOCK_TYPE_PULL || push->socket_type != SOCK_TYPE_PUSH) return E_SOCKET_WRONG_TYPE;

    uint16_t request_ptr = pull->requeste_ptr;
    uint16_t mask = pull->buffer_size-1;

    request_t* req = &pull->request_ring_buffer[request_ptr & mask];

    req->type = REQUEST_JOIN;
    req->length = length;
    req->request.push_to = push;

    push->drb_for_join = drb;
    // push->joined = 1;

    pull->requested_bytes +=length;
    return socket_internal_set_and_notify(&pull->requeste_ptr,
                                          request_ptr+1,
                                          &pull->fulfiller_component.fulfiller_waiting);
}

// This mad request will try join pull->push for length bytes, but will block neither of them.
// Instead a proxy request queue (that you have both ends of) is used as an intermediate.
// The proxy queue cannot be used until this is finished
// It IS (somewhat) safe to use the same proxy queue for the same pull/push pair even if the first proxy_join has not finished
// You could also use some barriers (if implemented) to queue up more uses of the same proxy without a wait

ssize_t socket_request_proxy_join(requester_t pull, requester_t proxy_req,
                                           data_ring_buffer* drb, uint64_t length, uint32_t drb_off_pull,
                                           requester_t push, fulfiller_t proxy_full, uint32_t drb_off_push) {

    uni_dir_socket_fulfiller* pf = UNSEAL_CHECK_FULFILLER(proxy_full);
    if(!pf) return E_BAD_SEAL;

    if((size_t)pf->requester != (size_t)proxy_req) return E_NOT_CONNECTED;

    ssize_t res = socket_request_join(pull, proxy_req, drb, length, drb_off_pull);

    if(res != 0) return res;

    return socket_request_proxy(push, proxy_full, length, drb_off_push);
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


    if(size >= sizeof(capability) && align != (uint64_t)(-1)) {
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

ssize_t socket_drb_space_alloc(data_ring_buffer* data_buffer, uint64_t align, uint64_t size, int dont_wait,
                               char** c1, char**c2, size_t* part1_out, requester_t r){
    uni_dir_socket_requester* requester = NULL;

    if(r) {
        requester = UNSEAL_CHECK_REQUESTER(r);
        if(!requester) return E_BAD_SEAL;
    }

    return socket_internal_drb_space_alloc(data_buffer, align,size, dont_wait, c1, c2, part1_out, requester);
}

uint8_t socket_requester_get_type(requester_t requester) {
    uni_dir_socket_requester* r = UNSEAL_CHECK_REQUESTER(requester);
    return r ? r->socket_type : SOCK_TYPE_ER;
}

uint8_t socket_fulfiller_get_type(fulfiller_t fulfiller) {
    uni_dir_socket_fulfiller* f = UNSEAL_CHECK_FULFILLER(fulfiller);
    return f ? f->socket_type : SOCK_TYPE_ER;
}

ssize_t socket_request_ind_db(requester_t r, const char* buf, uint32_t size,
                                       data_ring_buffer* data_buffer,
                                       int dont_wait, register_t perms) {
    ssize_t res;

    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);

    if(!requester) return E_BAD_SEAL;

    if(!data_buffer->buffer) return E_NO_DATA_BUFFER;

    if(size + sizeof(capability) > data_buffer->buffer_size) return E_MSG_SIZE;

    if(size == 0) return 0;

    char* cap1;
    char* cap2;
    size_t part_1;
    assert(data_buffer->partial_length == 0);

    res = socket_internal_drb_space_alloc(data_buffer, (uint64_t)buf, size, dont_wait, &cap1, &cap2, &part_1, requester);

    if(res < 0) return res;

    uint64_t align_off = res;

    uint8_t sock_type = requester->socket_type;

    if(sock_type == SOCK_TYPE_PUSH) {
        memcpy(cap1, buf, part_1);
    }
    cap1 = cheri_andperm(cap1, perms);

    res = socket_internal_request_ind(requester, cap1, part_1, part_1 + align_off);
    if(res < 0) return res;

    if(cap2) {
        size_t part_2 = size - part_1;
        if(sock_type == SOCK_TYPE_PUSH) {
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

ssize_t socket_request_oob(requester_t r, request_type_e r_type, intptr_t oob_val, uint64_t length, uint32_t drb_off) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);

    if(!requester) return E_BAD_SEAL;

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

static void socket_internal_dump_requests(uni_dir_socket_requester* requester) {
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

void socket_dump_requests(requester_t requester) {
    uni_dir_socket_requester* r = UNSEAL_CHECK_REQUESTER(requester);
    if(!r) {
        printf("BAD REQUESTER\n");
        return;
    }
    socket_internal_dump_requests(r);
}

static void socket_internal_dump_fulfiller(uni_dir_socket_fulfiller* f) {

    printf("Fulfiller: fp:%x(%lx). mp:%x(%lx). hd %x\n",
           f->requester->fulfiller_component.fulfill_ptr,
           f->partial_fulfill_bytes,
           f->fulfill_mark_ptr,
           f->partial_fulfill_mark_bytes,
           f->requester->requeste_ptr);

    socket_internal_dump_requests(f->requester);
}

void socket_dump_fulfiller(fulfiller_t fulfiller) {
    uni_dir_socket_fulfiller* f = UNSEAL_CHECK_FULFILLER(fulfiller);

    if(!f) {
        printf("BAD FULFILLER\n");
        return;
    }
    socket_internal_dump_fulfiller(fulfiller);

}
// This will fulfill n bytes, progress if progress is set (otherwise this is a peek), and check if check is set.
// Visit will be called on each buffer, with arguments arg, length (for the buffer) and offset + previous lengths
// oob_visit will be called on out of band requests. If oob_visit is null, fulfill stops and E_OOB is returned if
// no fulfillment was made

// If set mark is on (only allowed when peeking) then a mark is placed where progress would otherwise be set to
// When peeking it is allowed to start from the last mark (but not when fulfilling)
// Optionally pass an auth_t. When a locked reference is found try use auth

// FIXME: Make sure everything is safe manually

static ssize_t socket_internal_fulfill_progress_bytes_impl(uni_dir_socket_fulfiller* fulfiller, size_t bytes,
                                               enum FULFILL_FLAGS flags,
                                               ful_func* visit, capability arg, uint64_t offset,
                                               ful_oob_func* oob_visit, ful_sub* sub_visit, capability data_arg, capability oob_data_arg,
                                               found_id_t* for_auth) {

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
        uni_dir_socket_requester* push_to;

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
            // FIXME: We might need to reset the mark the first time we proxy
            if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill proxy\n");
            proxy = req->request.proxy_for;
            ret = socket_internal_fulfill_progress_bytes_impl(proxy, bytes_to_process,
                                                         flags | F_IN_PROXY,
                                                         visit, arg, offset, oob_visit, sub_visit,
                                                         data_arg, oob_data_arg, for_auth);
        } else if(req->type == REQUEST_JOIN) {

            if(flags & F_CANCEL_NON_OOB) break;

            push_to = req->request.push_to;
            ssize_t sub_ret;

            if(sub_visit) {
                ret = 0;
                if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill join with sub visit\n");
                // User can sub their own buffers. Keep getting them and pushing as requests
                char* user_buf;

                while(ret != bytes_to_process) {

                    sub_ret = socket_internal_requester_space_wait(push_to, 1, flags & F_DONT_WAIT, 0);

                    if(sub_ret < 0) break;

                    sub_ret = INVOKE_FUNCTION_POINTER(sub_visit,data_arg,arg, offset + ret, bytes_to_process - ret, &user_buf);

                    if(sub_ret <= 0) break;

                    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill join makes request\n");
                    socket_internal_request_ind(push_to, user_buf, (uint64_t)sub_ret, 0);

                    ret +=sub_ret;
                }

            } else if(visit) {
                ret = 0;

                if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill join with normal visit\n");

                // User can only put data in buffers. Use the drb instead.
                data_ring_buffer* drb = push_to->drb_for_join;

                // Unless we hijack this pointer this needs to be true
                assert((size_t)&drb->fulfill_ptr == (size_t)push_to->drb_fulfill_ptr);

                sub_ret = E_NO_DATA_BUFFER;

                if(drb) {
                    ret = 0;
                    size_t chunk_size = drb->buffer_size / 2;

                    while(ret != bytes_to_process) {
                        uint64_t req_size = (chunk_size > (bytes_to_process-ret)) ? (bytes_to_process-ret) : chunk_size;

                        char* cap1;
                        char* cap2;
                        size_t part1;

                        sub_ret = socket_internal_drb_space_alloc(drb, drb->requeste_ptr, req_size,
                                                                  flags & F_DONT_WAIT, &cap1, &cap2, &part1, push_to);

                        if(sub_ret < 0) break;

                        uint64_t align_off = (uint64_t)sub_ret;
                        uint64_t size1 = part1;
                        uint64_t size2 = req_size - part1;

                        size_t visit_gave = 0;

                        sub_ret = INVOKE_FUNCTION_POINTER(visit, data_arg, arg, cap1, offset + ret, size1);

                        if(sub_ret > 0) {
                            socket_internal_request_ind(push_to, cap1, (size_t)sub_ret, (size_t)sub_ret+align_off);
                            visit_gave = (size_t)sub_ret;
                        }

                        if(sub_ret == size1 && cap2) {
                            sub_ret = visit(arg, cap2, offset + size1 + ret, size2);

                            if(sub_ret > 0) {
                                socket_internal_request_ind(push_to, cap2, (size_t)sub_ret, sub_ret);
                                visit_gave += sub_ret;
                            }
                        }

                        ret += visit_gave;

                        if(visit_gave != req_size) {
                            // We allocated more than the visit managed to give, so undo
                            size_t over_alloc = visit_gave == 0 ? (size_t)req_size : (size_t)(req_size + align_off - visit_gave);
                            drb->requeste_ptr -=over_alloc;
                            break;
                        }

                    }
                }
            }

            if(ret == 0) ret = sub_ret;

        } else {
            if (req->type == REQUEST_IM || req->type == REQUEST_IND) {
                if(flags & F_CANCEL_NON_OOB) break;
                if(visit) {
                    char *buf = (req->type == REQUEST_IM) ? req->request.im + partial_bytes : req->request.ind + partial_bytes;
                    ret = INVOKE_FUNCTION_POINTER(visit, data_arg, arg, buf, offset, bytes_to_process);
                }
            }
            if (req->type >= REQUEST_OUT_BAND) {
                if(flags & F_SKIP_OOB) {
                    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill Oob skip\n");
                    ret = bytes_to_process;
                } else if(oob_visit) {
                    if(SOCK_TRACING && (flags & F_TRACE)) printf("Sock fulfill Oob visit\n");
                    // FIXME pass the request by value!
                    ret = INVOKE_FUNCTION_POINTER(oob_visit, oob_data_arg, arg, req, offset, partial_bytes,
                                                  bytes_to_process);
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
                if(req->type == REQUEST_JOIN) {
                    // TODO we may wish to be able have the original owner sleep and be notified when this happens
                    // push_to->joined = 0;
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

ssize_t socket_fulfill_progress_bytes_authorised(fulfiller_t fulfiller, size_t bytes,
                                                    enum FULFILL_FLAGS flags,
                                                    cert_t cert, capability arg, uint64_t offset) {
    uni_dir_socket_fulfiller* f = UNSEAL_CHECK_FULFILLER(fulfiller);
    if(!f) return E_BAD_SEAL;

    cap_pair pair;
    found_id_t* id = rescap_check_cert(cert, &pair);
    ful_pack* pack = (ful_pack*)pair.data;
    return socket_internal_fulfill_progress_bytes_impl(f, bytes, flags, pack->ful, arg, offset, pack->ful_oob, pack->sub, pack->data_arg, pack->oob_data_arg, id);
}

ssize_t socket_fulfill_progress_bytes_unauthorised(fulfiller_t fulfiller, size_t bytes,
                                                    enum FULFILL_FLAGS flags,
                                                    ful_func* visit, capability arg, uint64_t offset,
                                                    ful_oob_func* oob_visit, ful_sub* sub_visit, capability data_arg, capability oob_data_arg) {
    uni_dir_socket_fulfiller* f = UNSEAL_CHECK_FULFILLER(fulfiller);
    if(!f) return E_BAD_SEAL;

    return socket_internal_fulfill_progress_bytes_impl(f, bytes, flags, visit, arg, offset, oob_visit, sub_visit, data_arg, oob_data_arg, NULL);
}

int socket_fulfiller_reset_check(fulfiller_t f) {
    uni_dir_socket_fulfiller* fulfiller = UNSEAL_CHECK_FULFILLER(f);
    if(!fulfiller) return E_BAD_SEAL;
    if(fulfiller->proxy_times != fulfiller->proxy_fin_times) return E_IN_PROXY;
    fulfiller->partial_fulfill_mark_bytes = fulfiller->partial_fulfill_bytes;
    fulfiller->fulfill_mark_ptr = fulfiller->requester->fulfiller_component.fulfill_ptr;
    return 0;
}

uint64_t socket_requester_bytes_requested(requester_t r) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    return requester->requested_bytes - requester->fulfiller_component.fulfilled_bytes;
}

uint64_t socket_fulfiller_bytes_requested(fulfiller_t f) {
    uni_dir_socket_fulfiller* fulfiller = UNSEAL_CHECK_FULFILLER(f);
    return (fulfiller->requester->requested_bytes - fulfiller->requester->fulfiller_component.fulfilled_bytes);
}

static int socket_internal_fulfiller_init(uni_dir_socket_fulfiller* fulfiller, uint8_t socket_type) {
    fulfiller->socket_type = socket_type;
    fulfiller->guard.guard = MAKE_USER_GUARD_TYPE(fulfiller_guard_type);
    return 0;
}

static int socket_internal_requester_init(uni_dir_socket_requester* requester, uint16_t buffer_size, uint8_t socket_type, data_ring_buffer* paired_drb) {
    if(!is_power_2(buffer_size)) return E_BUFFER_SIZE_NOT_POWER_2;

    requester->fulfiller_component.guard.guard = MAKE_USER_GUARD_TYPE(requester_guard_type);

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

int socket_requester_set_drb_ptr(requester_t r, uint64_t* drb_ptr) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    if(!requester) return E_BAD_SEAL;

    requester->drb_fulfill_ptr = drb_ptr;

    return 0;
}

int socket_requester_set_drb(requester_t r, struct data_ring_buffer* drb) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    if(!requester) return E_BAD_SEAL;

    requester->drb_fulfill_ptr = &drb->fulfill_ptr;

    return 0;
}

ERROR_T(requester_t) socket_new_requester(res_t res, uint16_t buffer_size, uint8_t socket_type, data_ring_buffer* paired_drb) {
    cap_pair pair;

    rescap_take(res, &pair);

    if(pair.data == NULL || (cheri_getlen(pair.data) < SIZE_OF_request(buffer_size))) return MAKE_ER(requester_t, E_BAD_RESERVATION);

    uni_dir_socket_requester* requester = (uni_dir_socket_requester*)pair.data;
    int result = socket_internal_requester_init(requester, buffer_size, socket_type, paired_drb);

    if(result != 0) return MAKE_ER(requester_t, result);

    return MAKE_VALID(requester_t,cheri_seal(requester, get_cds()));


}

ERROR_T(fulfiller_t) socket_new_fulfiller(res_t res, uint8_t socket_type) {
    cap_pair pair;

    rescap_take(res, &pair);

    if(pair.data == NULL || (cheri_getlen(pair.data) < sizeof(uni_dir_socket_fulfiller))) return MAKE_ER(fulfiller_t, E_BAD_RESERVATION);

    uni_dir_socket_fulfiller* f = (uni_dir_socket_fulfiller*)pair.data;
    int result = socket_internal_fulfiller_init(f, socket_type);

    if(result != 0) return MAKE_ER(fulfiller_t, result);

    return MAKE_VALID(fulfiller_t,cheri_seal(f, get_cds()));
}

// FIXME: stuff to with making sure its not in use etc.

int socket_reuse_requester(requester_t r, uint16_t buffer_size, uint8_t socket_type, data_ring_buffer* paired_drb) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    if(!requester) return E_BAD_SEAL;

    if(requester->connected && !requester->requester_closed) return E_SOCKET_CLOSED;

    if(cheri_getlen(requester) < SIZE_OF_request(buffer_size)) return E_BAD_RESERVATION;

    bzero(r, SIZE_OF_request(requester->buffer_size));

    return socket_internal_requester_init(requester, buffer_size, socket_type, paired_drb);
}

int socket_reuse_fulfiller(fulfiller_t f, uint8_t socket_type) {
    uni_dir_socket_fulfiller* fulfiller = UNSEAL_CHECK_FULFILLER(f);
    if(!fulfiller) return E_BAD_SEAL;

    // TODO SAFE
    // if(fulfiller->connected && !fulfiller->requester->fulfiller_component.fulfiller_closed) return E_SOCKET_CLOSED:

    bzero(fulfiller, SIZE_OF_fulfill);

    return socket_internal_fulfiller_init(fulfiller, socket_type);
}

// Now not only read only but sealed by the type of the domain from which this is run
static uni_dir_socket_requester*  socket_internal_make_read_only(uni_dir_socket_requester* requester) {
    size_t size = SIZE_OF_request(requester->buffer_size);
    return (uni_dir_socket_requester*)cheri_andperm(requester, CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
}

requester_t socket_make_ref_for_fulfill(requester_t r) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    requester = socket_internal_make_read_only(requester);
    return (requester_t)cheri_seal(requester, get_cds());
}

int socket_fulfiller_connect(fulfiller_t f, requester_t r) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    uni_dir_socket_fulfiller* fulfiller = UNSEAL_CHECK_FULFILLER(f);

    if(!requester || !fulfiller) return E_BAD_SEAL;

    if(fulfiller->connected) return E_ALREADY_CONNECTED;

    if(requester->socket_type != fulfiller->socket_type) return E_SOCKET_WRONG_TYPE;

    fulfiller->requester = requester;
    fulfiller->connected = 1;
    return 0;
}

int socket_requester_connect(requester_t r) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    if(!requester) return E_BAD_SEAL;

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
    : [res]"=&C"(waiter)
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

ssize_t socket_close_requester(requester_t r, int wait_finish, int dont_wait) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    if(!requester) return E_BAD_SEAL;

    if(wait_finish) {
        ssize_t ret = socket_requester_wait_all_finish(r, dont_wait);
        // Its O.K for the other end to close if all requests are done
        if(ret < 0) return ret;
    }
    return socket_internal_close_safe(&requester->requester_closed,
                                      &requester->fulfiller_component.fulfiller_closed,
                                      &requester->fulfiller_component.fulfiller_waiting);
}

ssize_t socket_close_fulfiller(fulfiller_t f, int wait_finish, int dont_wait) {
    uni_dir_socket_fulfiller* fulfiller = UNSEAL_CHECK_FULFILLER(f);
    if(!fulfiller) return E_BAD_SEAL;

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

static void socket_internal_fulfill_cancel_wait(uni_dir_socket_fulfiller* fulfiller) {
    uni_dir_socket_requester_fulfiller_component* access = NULL;
    VMEM_SAFE_DEREFERENCE(&fulfiller->requester->access, access, c);
    if(access == NULL) return;
    access->fulfiller_waiting = NULL;
    if(fulfiller->proxy_times != fulfiller->proxy_fin_times) fulfiller->proxyied_in->fulfiller_component.requester_waiting = NULL;
}

static void socket_internal_request_cancel_wait(uni_dir_socket_requester* requester) {
    requester->fulfiller_component.requester_waiting = NULL;
}

static enum poll_events socket_internal_fulfill_poll(uni_dir_socket_fulfiller* fulfiller, enum poll_events io, int set_waiting, int from_check, int in_proxy) {
    // Wait until there is something to fulfill and we are not proxying
    enum poll_events ret = POLL_NONE;


    if(io) {

        if(!fulfiller->connected) return POLL_NVAL;

        if(!set_waiting) socket_internal_fulfill_cancel_wait(fulfiller);

        ssize_t wait_res = 0;

        if(!in_proxy) {
            wait_res = socket_internal_fulfiller_wait_proxy(fulfiller, !set_waiting, 1);
        }

        if(wait_res == 0) {
            uint16_t amount = 1;
            // We wait for an amount to be present such there is something past our checkpoint
            if(from_check) amount +=
                                   (fulfiller->fulfill_mark_ptr - fulfiller->requester->fulfiller_component.fulfill_ptr) & (fulfiller->requester->buffer_size-1);

            wait_res = socket_internal_fulfill_outstanding_wait(fulfiller, amount, !set_waiting, 1);

            if(wait_res == 0) {
                request_t *req = &fulfiller->requester->request_ring_buffer[
                        (from_check ? fulfiller->fulfill_mark_ptr : fulfiller->requester->fulfiller_component.fulfill_ptr)
                        & (fulfiller->requester->buffer_size-1)];
                if(req->type == REQUEST_PROXY) {
                    uni_dir_socket_fulfiller* proxy = req->request.proxy_for;
                    enum poll_events proxy_events = socket_internal_fulfill_poll(proxy, io, set_waiting, from_check, 1);
                    ret |= proxy_events;
                } else ret |= io;
            }

        }

        if(wait_res == E_SOCKET_CLOSED) ret |= POLL_HUP;
        else if(wait_res == E_AGAIN || wait_res == E_IN_PROXY) return ret;
        else if(wait_res < 0) ret |= POLL_ER;

    } else {
        if(socket_internal_fulfiller_closed_safe(fulfiller)) ret |= POLL_HUP;
    }

    return ret;
}

enum poll_events socket_fulfill_poll(fulfiller_t f, enum poll_events io, int set_waiting, int from_check, int in_proxy) {
    uni_dir_socket_fulfiller* fulfiller = UNSEAL_CHECK_FULFILLER(f);
    if(!fulfiller) return POLL_NVAL;

    return socket_internal_fulfill_poll(fulfiller, io, set_waiting, from_check, in_proxy);
}

enum poll_events socket_request_poll(requester_t r, enum poll_events io, int set_waiting, uint16_t space) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);

    if(!requester) return POLL_NVAL;

    // Wait until there is something to request

    enum poll_events ret = POLL_NONE;

    if(!requester->connected) return POLL_NVAL;

    if(space == SPACE_AMOUNT_ALL) space = requester->buffer_size;

    if(io) {
        if(!set_waiting) socket_internal_request_cancel_wait(requester);

        int wait_res = socket_internal_requester_space_wait(requester,space, !set_waiting, 1);

        if(wait_res == 0) ret |= io;
        else if(wait_res == E_AGAIN) return ret;
        else if(wait_res < 0) ret |= POLL_ER;
    } else {
        if(requester->requester_closed || requester->fulfiller_component.fulfiller_closed) ret |= POLL_HUP;
    }

    return ret;
}

int in_proxy(fulfiller_t f) {
    uni_dir_socket_fulfiller* fulfiller = UNSEAL_CHECK_FULFILLER(f);
    if(!fulfiller) return E_BAD_SEAL;
    return (fulfiller->proxy_times != fulfiller->proxy_fin_times);
}

// Some useful fulfillment functions

ssize_t TRUSTED_CROSS_DOMAIN(copy_in)(capability user_buf, char* req_buf, uint64_t offset, uint64_t length);
ssize_t copy_in(capability user_buf, char* req_buf, uint64_t offset, uint64_t length) {
    memcpy(req_buf,(char*)user_buf+offset, length);
    return (ssize_t)length;
}

ssize_t copy_out(capability user_buf, char* req_buf, uint64_t offset, uint64_t length) {
    memcpy((char*)user_buf+offset, req_buf, length);
    return (ssize_t)length;
}

ssize_t copy_out_no_caps(capability user_buf, char* req_buf, uint64_t offset, uint64_t length) {
    req_buf = (char*)cheri_andperm(req_buf, CHERI_PERM_LOAD);
    memcpy((char*)user_buf+offset, req_buf, length);
    return (ssize_t)length;
}

struct fwf_args {
    uni_dir_socket_fulfiller* writer;
    int dont_wait;
};

ssize_t TRUSTED_CROSS_DOMAIN(socket_fulfill_with_fulfill)(capability arg, char* buf, uint64_t offset, uint64_t length);
ssize_t socket_fulfill_with_fulfill(capability arg, char* buf, uint64_t offset, uint64_t length) {
    struct fwf_args* args = (struct fwf_args*)arg;
    // TODO: We can probably avoid a copy for join requests here
    ful_func * ff = &(TRUSTED_CROSS_DOMAIN(copy_in)); // Will be called only from within library, we pass trusted entry.
    return socket_internal_fulfill_progress_bytes_impl(args->writer, length, F_CHECK | F_PROGRESS | args->dont_wait, ff, (capability)buf, 0, NULL, NULL, get_ctl(), NULL, NULL);
}

ssize_t socket_fulfill_progress_bytes_soft_join(fulfiller_t push_read, fulfiller_t pull_write, size_t bytes, enum FULFILL_FLAGS flags) {
    uni_dir_socket_fulfiller* push = UNSEAL_CHECK_FULFILLER(push_read);
    uni_dir_socket_fulfiller* pull = UNSEAL_CHECK_FULFILLER(pull_write);

    if(!push || !push) return E_BAD_SEAL;

    _safe struct fwf_args args;
    args.writer = pull;
    args.dont_wait = flags & F_DONT_WAIT;

    return socket_internal_fulfill_progress_bytes_impl(push, bytes, flags,
            &TRUSTED_CROSS_DOMAIN(socket_fulfill_with_fulfill), &args, 0, NULL,NULL,
            TRUSTED_DATA, NULL, NULL);

}

uint8_t socket_requester_is_fulfill_closed(requester_t r) {
    uni_dir_socket_requester* requester = UNSEAL_CHECK_REQUESTER(r);
    return requester->fulfiller_component.fulfiller_closed;
}
