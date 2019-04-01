/*-
 * Copyright (c) 2019 Lawrence Esswood
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

#ifndef SOCKETLIB_IF_H
#define SOCKETLIB_IF_H

#include "dylink.h"
#include "nano/nanotypes.h"
#include "string.h"
#include "mman.h"

// All the functions that this library exports

#define SOCKET_LIB_IF_LIST(ITEM, ...)\
/* Call after object init in your own library */\
    ITEM(init_external_thread, int, (act_control_kt self_ctrl, mop_t mop, queue_t* queue, startup_flags_e start_flags), __VA_ARGS__)\
/**/\
    ITEM(socket_new_requester, ERROR_T(requester_t),  (res_t res, uint16_t buffer_size, uint8_t socket_type, data_ring_buffer* paired_drb),__VA_ARGS__)\
/**/\
    ITEM(socket_new_fulfiller, ERROR_T(fulfiller_t), (res_t res, uint8_t socket_type),__VA_ARGS__)\
    ITEM(socket_reuse_requester, int, (requester_t r, uint16_t buffer_size, uint8_t socket_type, data_ring_buffer* paired_drb), __VA_ARGS__)\
    ITEM(socket_reuse_fulfiller, int, (fulfiller_t f, uint8_t socket_type), __VA_ARGS__)\
/**/\
    ITEM(socket_fulfiller_connect, int, (fulfiller_t fulfiller, requester_t requester),__VA_ARGS__)\
/**/\
    ITEM(socket_requester_connect, int, (requester_t requester),__VA_ARGS__)\
/**/\
    ITEM(socket_make_ref_for_fulfill, requester_t, (requester_t requester),__VA_ARGS__)\
/**/\
    ITEM(socket_fulfill_progress_bytes_authorised, ssize_t, (fulfiller_t fulfiller, size_t bytes, enum FULFILL_FLAGS flags, cert_t cert, capability arg, uint64_t offset),__VA_ARGS__)\
/**/\
    ITEM(socket_fulfill_progress_bytes_unauthorised, ssize_t, (fulfiller_t fulfiller, size_t bytes, enum FULFILL_FLAGS flags, ful_func* visit, capability arg, uint64_t offset, ful_oob_func* oob_visit, ful_sub* sub_visit, capability data_arg, capability oob_data_arg),__VA_ARGS__)\
/* Wait for all requests to be marked as fulfilled */\
    ITEM(socket_requester_wait_all_finish, ssize_t, (requester_t * r, int dont_wait),__VA_ARGS__)\
/**/\
    ITEM(socket_request_im, ssize_t, (requester_t r, uint8_t length, char** buf_out, char* buf_in, uint32_t drb_off),__VA_ARGS__)\
/**/\
    ITEM(socket_request_ind, ssize_t,  (requester_t r, char* buf, uint64_t length, uint32_t drb_off),__VA_ARGS__)\
/**/\
    ITEM(socket_request_proxy, ssize_t, (requester_t r, fulfiller_t f, uint64_t length, uint32_t drb_off),__VA_ARGS__)\
/**/\
    ITEM(socket_request_join, ssize_t, (requester_t pull_req, requester_t push_req, data_ring_buffer* drb, uint64_t length, uint32_t drb_off),__VA_ARGS__)\
/**/\
    ITEM(socket_request_proxy_join, ssize_t, (requester_t pull, requester_t proxy_req, data_ring_buffer* drb, uint64_t length, uint32_t drb_off_pull, requester_t push, fulfiller_t proxy_full, uint32_t drb_off_push),__VA_ARGS__)\
/**/\
    ITEM(socket_drb_space_alloc, ssize_t, (data_ring_buffer* data_buffer, uint64_t align, uint64_t size, int dont_wait, char** c1, char**c2, size_t* part1_out, requester_t r),__VA_ARGS__)\
/*Requests length bytes as an ind request, but uses a data buffer instead of the provided buf. For a write this does what you expect, copying the source buffer. For a read this will only use your buffer as an alignment hint*/\
    ITEM(socket_request_ind_db, ssize_t, (requester_t r, const char* buf, uint32_t size, data_ring_buffer* data_buffer, int dont_wait, register_t perms),__VA_ARGS__)\
/**/\
    ITEM(socket_request_oob, ssize_t, (requester_t r, request_type_e r_type, intptr_t oob_val, uint64_t length, uint32_t drb_off),__VA_ARGS__)\
/**/\
    ITEM(socket_request_poll, enum poll_events, (requester_t requester, enum poll_events io, int set_waiting, uint16_t space),__VA_ARGS__)\
/**/\
    ITEM(socket_fulfill_poll, enum poll_events, (fulfiller_t fulfiller, enum poll_events io, int set_waiting, int from_check, int in_proxy),__VA_ARGS__)\
/* Wait for enough space for 'need_space' requests */\
    ITEM(socket_requester_space_wait, int, (requester_t requester, uint16_t need_space, int dont_wait, int delay_sleep),__VA_ARGS__)\
/* Wait for 'amount' requests to be outstanding*/\
    ITEM(socket_fulfiller_outstanding_wait, int,  (fulfiller_t fulfiller, uint16_t amount, int dont_wait, int delay_sleep),__VA_ARGS__)\
/**/\
    ITEM(socket_dump_requests, void, (requester_t requester),__VA_ARGS__)\
/**/\
    ITEM(socket_dump_fulfiller, void, (fulfiller_t fulfiller),__VA_ARGS__)\
/**/\
    ITEM(socket_close_requester, ssize_t, (requester_t requester, int wait_finish, int dont_wait),__VA_ARGS__)\
/**/\
    ITEM(socket_close_fulfiller, ssize_t, (fulfiller_t fulfiller, int wait_finish, int dont_wait),__VA_ARGS__)\
    ITEM(socket_requester_is_fulfill_closed, uint8_t, (requester_t r), __VA_ARGS__)\
/**/\
    ITEM(socket_requester_get_type, uint8_t, (requester_t requester),__VA_ARGS__)\
/**/\
    ITEM(socket_fulfiller_get_type, uint8_t, (fulfiller_t fulfiller),__VA_ARGS__)\
/**/\
    ITEM(socket_fulfiller_reset_check, int, (fulfiller_t f),__VA_ARGS__)\
/**/\
    ITEM(socket_requester_bytes_requested, uint64_t, (requester_t f),__VA_ARGS__)\
    ITEM(socket_fulfiller_bytes_requested, uint64_t, (fulfiller_t f), __VA_ARGS__)\
/**/\
    ITEM(socket_requester_set_drb, int, (requester_t requester, struct data_ring_buffer* drb) , __VA_ARGS__)\
    ITEM(socket_requester_set_drb_ptr, int, (requester_t r, uint64_t* drb_ptr), __VA_ARGS__)\
    ITEM(in_proxy, int, (fulfiller_t f), __VA_ARGS__)\
/*Some Usefull fulfillment functions. These are in the libraries domain so use its */\
    ITEM(ful_oob_func_skip_oob, ssize_t, (capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length), __VA_ARGS__)\
    ITEM(ful_func_cancel_non_oob, ssize_t, (capability arg, char* buf, uint64_t offset, uint64_t length), __VA_ARGS__)\
    ITEM(copy_in, ssize_t, (capability user_buf, char* req_buf, uint64_t offset, uint64_t length), __VA_ARGS__)\
    ITEM(copy_out, ssize_t, (capability user_buf, char* req_buf, uint64_t offset, uint64_t length) , __VA_ARGS__)\
    ITEM(copy_out_no_caps, ssize_t, (capability user_buf, char* req_buf, uint64_t offset, uint64_t length), __VA_ARGS__)\
    ITEM(socket_fulfill_with_fulfill, ssize_t, (capability arg, char* buf, uint64_t offset, uint64_t length), __VA_ARGS__)\
    ITEM(socket_fulfill_progress_bytes_soft_join, ssize_t, (fulfiller_t push_read, fulfiller_t pull_write, size_t bytes, enum FULFILL_FLAGS flags), __VA_ARGS__)

// Currently dont_wait and recv with a pull socket interact badly. This ignores the don't wait flag.
#define FORCE_WAIT_SOCKET_RECV 1

#define SOCKET_CONNECT_IPC_NO       (0xbeef)

#define E_AGAIN                     (-1)
#define E_OOB                       (-17)
#define E_MSG_SIZE                  (-2)
#define E_COPY_NEEDED               (-11)
#define E_UNSUPPORTED               (-12)

#define E_CONNECT_FAIL              (-3)
#define E_CONNECT_FAIL_WRONG_PORT   (-9)
#define E_CONNECT_FAIL_WRONG_TYPE   (-10)

#define E_BUFFER_SIZE_NOT_POWER_2   (-4)
#define E_ALREADY_CLOSED            (-5)
#define E_SOCKET_CLOSED             (-6)
#define E_SOCKET_WRONG_TYPE         (-7)
#define E_SOCKET_NO_DIRECTION       (-8)

#define E_IN_PROXY                  (-13)
#define E_NO_DATA_BUFFER            (-14)
#define E_ALREADY_CONNECTED         (-15)
#define E_NOT_CONNECTED             (-16)
#define E_BAD_FLAGS                 (-18)
#define E_IN_JOIN                   (-19)

#define E_USER_FULFILL_ERROR        (-20)

#define E_AUTH_TOKEN_ERROR          (-21)
#define E_BAD_RESERVATION           (-22)
#define E_BAD_SEAL                  (-23)

#define SOCK_INF                    (uint64_t)(0xFFFFFFFFFFFFFFFULL)

typedef capability requester_t;
typedef capability fulfiller_t;

enum SOCKET_FLAGS {
    MSG_NONE                = 0x0,
    MSG_DONT_WAIT           = 0x1,
    MSG_NO_CAPS             = 0x2,
    MSG_NO_COPY             = 0x4,
    MSG_PEEK                = 0x8,
    MSG_EMULATE_SINGLE_PTR  = 0x10,
    SOCKF_GIVE_SOCK_N       = 0x20,
    MSG_TRACE               = 0x40,
// Close will assume it has to free the drb, write request, read request, and socket itself unless you specify these
    SOCKF_DRB_INLINE        = 0x80,
    SOCKF_SOCK_INLINE       = 0x100,
// Changes how poll behaves
    SOCKF_POLL_READ_MEANS_EMPTY = 0x800,
};

// Global enable/disable for socket tracing. Always uses syscall_printf for safety.
#define SOCK_TRACING        1

enum FULFILL_FLAGS {
    F_NONE                  = 0x0,
    F_DONT_WAIT             = 0x1, // Same as MSG_DONT_WAIT
    F_CHECK                 = 0x2,
    F_IN_PROXY              = 0x4,
    F_PROGRESS              = 0x8, // Same bit but opposite meaning to MSG_PEEK
    F_START_FROM_LAST_MARK  = 0x10,
    F_SET_MARK              = 0x20,
    F_TRACE                 = 0x40, // Same as MSG_TRACE
    F_APPLY_AUTH            = 0x80, // All capabilities passed to fulfill will be
};

#define SOCK_TYPE_PUSH 0
#define SOCK_TYPE_PULL 1
#define SOCK_TYPE_ER   (uint8_t)255

typedef enum {
    REQUEST_IM = 0,
    REQUEST_IND = 1,
    REQUEST_PROXY = 2, // Request that points at a fulfiller
    REQUEST_JOIN = 3, // Request that points at another requester
    REQUEST_BARRIER_TARGET = 4,
    REQUEST_BARRIER = (1 << 16),
    // Anything with 2 << 16 set are OOB and handled by the user. But some of these are pretty common so included here.
    REQUEST_OUT_BAND = (2 << 16),
    REQUEST_FLUSH = (2 << 16) + 1,
    REQUEST_SEEK = (2 << 16) + 2,
    REQUEST_SIZE = (2 << 16) + 3,
    REQUEST_CLOSE = (2 << 16) + 4, // Like flush, but we plan to close after
    REQUEST_TRUNCATE = (2 << 16) + 5,
    REQUEST_OUT_USER_START = (2 << 16) + (1 << 8) // To use in defining inhereting enums. Don't use as an OOB value
} request_type_e;

/* A socket is formed of a single requester and fulfiller. Between them they manage a ring buffer. The requester
 * enqueues requests, which are to remain valid until the fulfiller marks them as fulfilled, at which point the
 * fulfiller should no longer use them. There are currently 3 types of request. IM has some immediate data.
 * IND points to a buffer of data. Proxy points to another fulfiller, and asks to fulfill for them instead */

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

typedef union {
    intptr_t as_intptr_t;
    struct v_t {
        int64_t offset;
        int whence;
    } v;
} seek_desc;

typedef struct request {
    uint64_t length;
    uint32_t drb_fullfill_inc; // When this request is fulfilled, automagically bump a fulfillment for a data buffer
    request_type_e type;
    union {
        struct uni_dir_socket_fulfiller* proxy_for;
        char* ind;
        char im[CAP_SIZE];
        volatile act_notify_kt barrier_waiting;
        struct request* barrier_target;
        seek_desc seek_desc;
        intptr_t oob;
        struct uni_dir_socket_requester* push_to;
    } request;
} request_t;

_Static_assert(sizeof(request_t) ==  2*sizeof(capability), "Make sure each request type is small enough");

enum poll_events {
    POLL_NONE = 0,
    POLL_IN = 1,
    POLL_OUT = 2,
    POLL_ER = 4,
    POLL_HUP = 8,
    POLL_NVAL = 16,
};

#define POLLIN                     POLL_IN
#define POLLOUT                    POLL_OUT
#define POLLHUP                    POLL_HUP
#define POLLERR                    POLL_ER
#define POLLNVAL                   POLL_NVAL

#define SPACE_AMOUNT_ALL            (uint16_t)0xFFFF

//#define EPOLLET

// DONT USE THESE TYPES EXTERNALLY. THEY ARE HERE FOR SIZE ONLY

// Uni-directional socket.
typedef struct uni_dir_socket_requester_fulfiller_component  {
    guard_t guard; // if you intend to seal something you should include this
    volatile act_kt fulfiller_waiting;
    volatile act_kt requester_waiting;
    volatile uint64_t fulfilled_bytes;
    volatile uint16_t fulfill_ptr;
    volatile uint8_t  fulfiller_closed;
} uni_dir_socket_requester_fulfiller_component;

typedef struct uni_dir_socket_requester {
    uni_dir_socket_requester_fulfiller_component fulfiller_component;
    volatile uint8_t requester_closed;
    uint8_t socket_type;
    uint8_t connected;
    // uint8_t joined; // Currently being driven by someone else. Can't put in requests. TODO
    uint16_t buffer_size;           // Power of 2
    volatile uint16_t requeste_ptr;
    volatile uint64_t requested_bytes;
    volatile uint64_t* drb_fulfill_ptr;      // a pointer to a fulfilment pointer for a data buffer
    //found_id_t* data_for_foundation; // If not null then fulfillment functions must be signed with this id. TODO
    // sealing_cap data_seal            // If not null then everything we pass to fulfillment functions must be signed with this TODO
    uni_dir_socket_requester_fulfiller_component* access;
    struct data_ring_buffer* drb_for_join;
    // TODO: If we are in a join, point back to the other half so we can block properly.
    // TODO: Currently concurrent access is just undefined
    request_t request_ring_buffer[]; // Variable sized, is buffer_size.
} uni_dir_socket_requester;

struct requester_32 {
    uni_dir_socket_requester r;
    request_t pad[32];
};

typedef struct uni_dir_socket_fulfiller {
    guard_t guard; // if you intend to seal something you should include this
    uni_dir_socket_requester* requester;  // Read only. But this doesn't matter too much now that everything is in the same library
    volatile uint64_t partial_fulfill_bytes;
    volatile uint64_t partial_fulfill_mark_bytes;
    volatile uint16_t fulfill_mark_ptr;
    uint8_t socket_type;
    uint8_t connected;

    volatile uint16_t proxy_times;          // how many times proxied (can wrap)
    volatile uint16_t proxy_fin_times;      // how many times proxies have finished (can wrap)
    uni_dir_socket_requester* proxyied_in;  // set if proxied
} uni_dir_socket_fulfiller;

#define SIZE_OF_request(buffer_size) ((sizeof(request_t) * buffer_size) + sizeof(uni_dir_socket_requester))
#define SIZE_OF_fulfill sizeof(uni_dir_socket_fulfiller)

DEC_ERROR_T(requester_t);
DEC_ERROR_T(fulfiller_t);

// Ring buffer for copy in for unix abstraction or to not reveal original capability
typedef struct data_ring_buffer {
    volatile uint64_t requeste_ptr;
    volatile uint64_t fulfill_ptr;
    uint32_t buffer_size;
    uint32_t partial_length;
    char* buffer;
} data_ring_buffer;

// Call this to fulfill and / or progress. ful_func will be called on every char* to be fulfilled, providing its length
// an offset that is offset plus all previous lengths. arg is a user argument that is passed through
typedef ssize_t ful_func(capability arg, char* buf, uint64_t offset, uint64_t length);
typedef ssize_t ful_oob_func(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length);
// If you could provide your own buffers as well, strictly optional, but can be used to further avoid copies
typedef ssize_t ful_sub(capability arg, uint64_t offset, uint64_t length, char** out_buf);

// If you have the authority to act as a foundation sign this struct
typedef struct ful_pack_s {
    ful_func* ful;
    ful_oob_func * ful_oob;
    ful_sub* sub;
    capability data_arg;
    capability oob_data_arg;
} ful_pack;

#endif //SOCKETLIB_IF_H
