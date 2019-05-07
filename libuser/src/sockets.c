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



#include "sockets.h"
#include "stdlib.h"
#include "misc.h"

ALLOCATE_PLT_SOCKETS

// TODO do something similar to sockets.c in libsocket
#define printf

void dump_socket(unix_like_socket* sock) {
    if(sock->con_type & CONNECT_PUSH_WRITE) {
        printf("Push writer:\n");
        socket_dump_requests(sock->write.push_writer);
    }
    if(sock->con_type & CONNECT_PUSH_READ) {
        printf("Push read:\n");
        socket_dump_fulfiller(sock->read.push_reader);
    }
    if(sock->con_type & CONNECT_PULL_WRITE) {
        printf("pull writer:\n");
        socket_dump_fulfiller(sock->write.pull_writer);
    }
    if(sock->con_type & CONNECT_PULL_READ) {
        printf("pull read:\n");
        socket_dump_requests(sock->read.pull_reader);
    }
}

ssize_t socket_requester_lseek(requester_t requester, int64_t offset, int whence, int dont_wait) {
    ssize_t ret;
    if((ret = socket_requester_space_wait(requester, 1, dont_wait, 0)) != 0) return ret;
    seek_desc desc;
    desc.v.offset = offset;
    desc.v.whence = whence;

    return socket_request_oob(requester, REQUEST_SEEK, desc.as_intptr_t, 0, 0);
}

#define MAX_SOCKNS 64
uint8_t free_ns[MAX_SOCKNS];
uint8_t free_n;

// Not thread safe, bit of a hack to help nginx...
static uint8_t alloc_sockn(void) {
    static int init = 0;
    if(!init) {
        init = 1;
        for(uint8_t i = 0; i != MAX_SOCKNS;i++) {
            free_ns[i] = i+1;
        }
    }

    uint8_t res = free_n;

    assert(res != MAX_SOCKNS);

    free_n = free_ns[free_n];

    return res;
}

static void free_sockn(uint8_t n) {
    free_ns[n] = free_n;
    free_n = n;
}

int socket_listen_rpc(register_t port,
                      requester_t requester,
                      fulfiller_t fulfiller) {

    if(requester == NULL && fulfiller == NULL) return E_SOCKET_NO_DIRECTION;

    // THEIR connection type
    enum socket_connect_type con_type = CONNECT_NONE;
    requester_t ro_req = NULL;

    uint8_t req_type = SOCK_TYPE_ER;
    uint8_t ful_type = SOCK_TYPE_ER;

    if(requester) {
        req_type = socket_requester_get_type(requester);
        ro_req = socket_make_ref_for_fulfill(requester);
        if(req_type == SOCK_TYPE_PULL) {
            con_type |= CONNECT_PULL_WRITE;
        } else  if(req_type == SOCK_TYPE_PUSH){
            con_type |= CONNECT_PUSH_READ;
        }
    }

    if(fulfiller) {
        ful_type = socket_fulfiller_get_type(fulfiller);
        if(ful_type == SOCK_TYPE_PULL) {
            con_type |= CONNECT_PULL_READ;
        } else if(ful_type == SOCK_TYPE_PUSH) {
            con_type |= CONNECT_PUSH_WRITE;
        }
    }

    msg_t msg;
    pop_msg(&msg);

    if(msg.v0 != SOCKET_CONNECT_IPC_NO) {
        return E_CONNECT_FAIL;
    }

    if(msg.a0 != port) {
        message_reply((capability)E_CONNECT_FAIL_WRONG_PORT,0,0,msg.c1, 0);
        return (E_CONNECT_FAIL_WRONG_PORT);
    }

    if(msg.a1 != con_type) {
        // Send failure
        message_reply((capability)E_CONNECT_FAIL_WRONG_TYPE,0,0,msg.c1, 0);
        return (E_CONNECT_FAIL_WRONG_TYPE);
    }

    if(fulfiller) {
        if(msg.c3 == NULL) {
            message_reply((capability)E_CONNECT_FAIL,0,0,msg.c1, 0);
            return (E_CONNECT_FAIL);
        }
        int res = socket_fulfiller_connect(fulfiller, msg.c3);
        if(res != 0) return res;
    }

    // ACK receipt
    message_reply((capability)ro_req,0,0, msg.c1, 0);


    if(requester) {
        int res = socket_requester_connect(requester);
        return res;
    }

    // TODO should we add an extra ack?

    return 0;
}

int socket_connect_via_rpc(act_kt target, register_t port,
                           requester_t requester,
                           fulfiller_t fulfiller) {

    if(requester == NULL && fulfiller == NULL) return E_SOCKET_NO_DIRECTION;

    msg_t msg;

    enum socket_connect_type con_type = CONNECT_NONE;
    requester_t ro_req = NULL;

    if(requester) {
        ro_req = socket_make_ref_for_fulfill(requester);
        uint8_t req_type = socket_requester_get_type(requester);
        if(req_type == SOCK_TYPE_PULL) {
            con_type |= CONNECT_PULL_READ;
        } else if(req_type == SOCK_TYPE_PUSH) {
            con_type |= CONNECT_PUSH_WRITE;
        }
    }

    uint8_t ful_type = SOCK_TYPE_ER;

    if(fulfiller) {
        ful_type = socket_fulfiller_get_type(fulfiller);
        if(ful_type == SOCK_TYPE_PULL) {
            con_type |= CONNECT_PULL_WRITE;
        } else if(ful_type == SOCK_TYPE_PUSH) {
            con_type |= CONNECT_PUSH_READ;
        }
    }

    capability cap_result = message_send_c(port,con_type,0,0,(capability)ro_req, NULL, NULL, NULL, target, SYNC_CALL, SOCKET_CONNECT_IPC_NO);

    ERROR_T(requester_t) res = ER_T_FROM_CAP(requester_t, cap_result);

    // Check ACK
    if(!IS_VALID(res)) return (int)res.er;

    if(fulfiller) {
        if(res.val == NULL) {
            return E_CONNECT_FAIL;
        }
        socket_fulfiller_connect(fulfiller, res.val);
    }

    if(requester) socket_requester_connect(requester);

    return 0;
}

ssize_t socket_close(unix_like_socket* sock) {
    int dont_wait = sock->flags & MSG_DONT_WAIT;
    ssize_t ret;

    if(sock->flags & SOCKF_GIVE_SOCK_N) free_sockn(sock->sockn);

    if(sock->con_type & CONNECT_PULL_READ) {
        ret = socket_close_requester(sock->read.pull_reader, 1, dont_wait);
    } else if(sock->con_type & CONNECT_PUSH_READ) {
        ret = socket_close_fulfiller(sock->read.push_reader, 1, dont_wait);
    }

    if(ret < 0) return  ret;

    if(sock->con_type & CONNECT_PUSH_WRITE) {
        ret = socket_close_requester(sock->write.push_writer, 1, dont_wait);
    } else if(sock->con_type & CONNECT_PULL_WRITE) {
        ret = socket_close_fulfiller(sock->write.pull_writer, 1, dont_wait);
    }

    return ret;
}

int init_data_buffer(data_ring_buffer* buffer, char* char_buffer, uint32_t data_buffer_size) {
    if(!is_power_2(data_buffer_size)) return E_BUFFER_SIZE_NOT_POWER_2;
    if(((size_t)char_buffer) & (sizeof(capability)-1)) return E_BUFFER_SIZE_NOT_POWER_2; // TODO add different error code
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

    if(flags & SOCKF_GIVE_SOCK_N) sock->sockn = alloc_sockn();

    if(data_buffer) {
        return init_data_buffer(&sock->write_copy_buffer, data_buffer, data_buffer_size);
    } else {

        // If no data buffer is provided we may very well wait and cannot perform a copy
        if((con_type & (CONNECT_PUSH_WRITE | CONNECT_PULL_READ)) && (flags & MSG_DONT_WAIT)) return E_SOCKET_WRONG_TYPE;
        //if((con_type & CONNECT_PUSH_WRITE) && !(flags & MSG_NO_COPY)) return E_COPY_NEEDED;

        sock->write_copy_buffer.buffer = NULL;
        return 0;
    }
}

ssize_t socket_requester_request_wait_for_fulfill(requester_t requester, char* buf, uint64_t length) {
    int ret;

    ret = socket_requester_space_wait(requester, 1, 0, 0);

    if(ret < 0) return ret;

    socket_request_ind(requester, buf, length, 0);

    // Wait for everything to be consumed
    return socket_requester_wait_all_finish(requester, 0);
}

ssize_t socket_send(unix_like_socket* sock, const char* buf, size_t length, enum SOCKET_FLAGS flags) {
    // Need to copy to buffer in order not to expose buf

    ssize_t flush = socket_flush_drb(sock);
    if(flush < 0) return flush;

    if((sock->flags | flags) & MSG_EMULATE_SINGLE_PTR) catch_up_write(sock);

    int dont_wait;
    dont_wait = (flags | sock->flags) & MSG_DONT_WAIT;

    if((sock->flags | flags) & MSG_NO_CAPS) buf = cheri_andperm(buf, CHERI_PERM_LOAD);


    ssize_t ret = E_SOCKET_WRONG_TYPE;
    if(sock->con_type & CONNECT_PUSH_WRITE) {
        requester_t requester = sock->write.push_writer;

        if((sock->flags | flags) & MSG_NO_COPY) {
            if(dont_wait) return E_COPY_NEEDED; // We can't not copy and not wait for consumption

            if(SOCK_TRACING && (sock->flags & MSG_TRACE)) printf("Socket sending request and waiting for all to finish\n");
            ret = socket_requester_request_wait_for_fulfill(requester, cheri_setbounds(buf, length), length);
            if(ret >= 0) ret = length;

            return length;

        } else {

            register_t perms = CHERI_PERM_LOAD;
            if(!((flags | sock->flags) & MSG_NO_CAPS)) perms |= CHERI_PERM_LOAD_CAP;

            if(SOCK_TRACING && (sock->flags & MSG_TRACE)) printf("Socket sending a request\n");

            ret = socket_request_ind_db(requester, buf, length, &sock->write_copy_buffer, dont_wait, perms);
        }

    } else if(sock->con_type & CONNECT_PULL_WRITE) {
        fulfiller_t fulfiller = sock->write.pull_writer;
        ful_func * ff = OTHER_DOMAIN_FP(copy_in);
        enum FULFILL_FLAGS progress = (enum FULFILL_FLAGS)(((sock->flags | flags) & MSG_PEEK) ^ F_PROGRESS);
        enum FULFILL_FLAGS trace = (enum FULFILL_FLAGS)((sock->flags | flags) & MSG_TRACE);
        ret = socket_fulfill_progress_bytes_unauthorised(fulfiller, length,
                                                     F_CHECK | progress | dont_wait | trace,
                                                     ff, (capability)buf, 0, NULL, NULL, LIB_SOCKET_DATA, NULL);
    }

    if(ret > 0 && ((sock->flags | flags) & MSG_EMULATE_SINGLE_PTR)) sock->read_behind+=ret;
    return ret;
}

ssize_t socket_recv(unix_like_socket* sock, char* buf, size_t length, enum SOCKET_FLAGS flags) {

    ssize_t flush = socket_flush_drb(sock);
    if(flush < 0) return flush;

    if(((sock->flags | flags) & MSG_EMULATE_SINGLE_PTR)) catch_up_read(sock);

    int dont_wait;
    dont_wait = (flags | sock->flags) & MSG_DONT_WAIT;

    ssize_t ret = E_SOCKET_WRONG_TYPE;
    if(sock->con_type & CONNECT_PULL_READ) {
        requester_t requester = sock->read.pull_reader;

        // TODO this use case is confusing. If we put the user buffer in, we could return 0, have async fulfill and then
        // TODO numbers on further reads. However, if the user provided a different buffer on the second call this would
        // TODO break horribly.
        if((sock->flags | flags) & MSG_NO_COPY) {
#if (!FORCE_WAIT_SOCKET_RECV)
            //if(dont_wait) return E_COPY_NEEDED; // We can't not copy and not wait for consumption
#endif
            ret = socket_requester_request_wait_for_fulfill(requester, cheri_setbounds(buf, length), length);
            if(ret >= 0) ret = length;
        } else {
            ret = E_UNSUPPORTED;
        }

    } else if(sock->con_type & CONNECT_PUSH_READ) {
        fulfiller_t fulfiller = sock->read.push_reader;
        ful_func * ff = ((sock->flags | flags) & MSG_NO_CAPS) ? OTHER_DOMAIN_FP(copy_out_no_caps) : OTHER_DOMAIN_FP(copy_out);
        enum FULFILL_FLAGS progress = (enum FULFILL_FLAGS)(((sock->flags | flags) & MSG_PEEK) ^ F_PROGRESS);
        enum FULFILL_FLAGS trace = (enum FULFILL_FLAGS)((sock->flags | flags) & MSG_TRACE);
        ret = socket_fulfill_progress_bytes_unauthorised(fulfiller, length,
                                                     F_CHECK | progress | dont_wait | trace,
                                                     ff, (capability)buf, 0, NULL, NULL, LIB_SOCKET_DATA, NULL);
    }

    if(ret > 0 && ((sock->flags | flags) & MSG_EMULATE_SINGLE_PTR)) sock->write_behind+=ret;

    return ret;
}

ssize_t socket_sendfile(unix_like_socket* sockout, unix_like_socket* sockin, size_t count) {
    uint8_t in_type;
    uint8_t out_type;

    ssize_t flush = socket_flush_drb(sockout);
    if(flush < 0) return flush;
    flush = socket_flush_drb(sockin);
    if(flush < 0) return flush;

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
        requester_t pull_read = sockin->read.pull_reader;
        fulfiller_t pull_write = sockout->write.pull_writer;

        ret = socket_requester_space_wait(pull_read, 1, dont_wait, 0);

        if(ret >= 0) {
            ret = socket_request_proxy(pull_read, pull_write, count, 0);
            if(ret >= 0) ret = count;
        }

    } else if(in_type == SOCK_TYPE_PULL && out_type == SOCK_TYPE_PUSH) {
        // Create custom buffer and generate a request (or few) for each
        requester_t pull_read = sockin->read.pull_reader;
        requester_t push_write = sockout->write.push_writer;

        ret = socket_requester_space_wait(pull_read, 1, dont_wait, 0);

        if(ret >= 0) {
            ret = socket_request_join(pull_read, push_write, &sockout->write_copy_buffer, count, 0);
            if(ret >= 0) ret = count;
        }

    } else if(in_type == SOCK_TYPE_PUSH && out_type == SOCK_TYPE_PULL) {
        // fullfill one read with a function that fulfills write
        fulfiller_t push_read = sockin->read.push_reader;
        fulfiller_t pull_write = sockout->write.pull_writer;

        ret = socket_fulfill_progress_bytes_soft_join(push_read, pull_write, count, F_CHECK | F_PROGRESS | dont_wait);

    } else if(in_type == SOCK_TYPE_PUSH && out_type == SOCK_TYPE_PUSH) {
        // Proxy
        fulfiller_t push_read = sockin->read.push_reader;
        requester_t push_write = sockout->write.push_writer;

        ret = socket_requester_space_wait(push_write, 1, dont_wait, 0);

        if(ret >= 0) {
            socket_request_proxy(push_write, push_read, count, 0);
            if(ret >= 0) ret = count;
        }
    }

    if(ret > 0) {
        if(em_w) sockout->read_behind+=ret;
        if(em_r) sockin->write_behind+=ret;
    }

    return ret;
}

void catch_up_write(unix_like_socket* file) {
    if(file->write_behind) {
        if(SOCK_TRACING && (file->flags & MSG_TRACE)) printf("Socket catching up write ptr by %lx\n", file->write_behind);
        socket_requester_lseek(file->write.push_writer,
                                        file->write_behind, SEEK_CUR, file->flags & MSG_DONT_WAIT);
        file->write_behind = 0;
    }
}

void catch_up_read(unix_like_socket* file) {
    if(file->read_behind) {
        if(SOCK_TRACING && (file->flags & MSG_TRACE)) printf("Socket catching up read ptr by %lx\n", file->read_behind);
        socket_requester_lseek(file->read.pull_reader,
                                        file->read_behind, SEEK_CUR, file->flags & MSG_DONT_WAIT);
        file->read_behind = 0;
    }
}

ssize_t socket_flush_drb(unix_like_socket* socket) {
    uint32_t len = socket->write_copy_buffer.partial_length;
    if(socket->write_copy_buffer.buffer && len != 0) {
        if(SOCK_TRACING && (socket->flags & MSG_TRACE)) printf("Socket flushing drb with %x bytes\n", len);
        if(socket->flags & MSG_EMULATE_SINGLE_PTR) catch_up_write(socket);
        // FIXME respect dont_wait
        // FIXME handle error returns
        char* c1;
        char* c2;
        size_t p1;
        ssize_t align_extra = socket_drb_space_alloc(&socket->write_copy_buffer,
                                                              (uint64_t)-1,
                                                              len,
                                                              socket->flags & MSG_DONT_WAIT,
                                                              &c1,
                                                              &c2,
                                                              &p1,
                                                              socket->write.push_writer);

        if(align_extra < 0) return align_extra;

        socket->write_copy_buffer.partial_length = 0;

        socket_request_ind(socket->write.push_writer, c1, p1, p1);
        if(p1 != len) {
            socket_request_ind(socket->write.push_writer, c2, len - p1, len - p1);
        }
        return len;
    }
    return 0;
}

static enum poll_events be_waiting_for_event(unix_like_socket* sock, enum poll_events asked_events, int set_waiting) {

    enum poll_events read = asked_events & POLL_IN;
    enum poll_events write = asked_events & POLL_OUT;

    enum poll_events ret = POLL_NONE;


    if(sock->con_type & CONNECT_PUSH_WRITE) {
        requester_t push_write = sock->write.push_writer;
        ret |= socket_request_poll(push_write, write, set_waiting, 1);
    } else if(sock->con_type & CONNECT_PULL_WRITE) {
        fulfiller_t pull_write = sock->write.pull_writer;
        ret |= socket_fulfill_poll(pull_write, write, set_waiting, 0, 0);
    } else if(write) return POLL_NVAL;

    if(sock->con_type & CONNECT_PUSH_READ) {
        fulfiller_t push_read = sock->read.push_reader;
        ret |= socket_fulfill_poll(push_read, read, set_waiting, 0, 0);
    } else if(sock->con_type & CONNECT_PULL_READ) {
        requester_t pull_read = sock->read.pull_reader;
        ret |= socket_request_poll(pull_read, read, set_waiting,
                                            sock->flags & SOCKF_POLL_READ_MEANS_EMPTY ? SPACE_AMOUNT_ALL : (uint16_t)1);
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

            if(sock_poll->fd == NULL) {
                sock_poll->revents = (enum poll_events)0;
                continue;
            }

            enum poll_events asked_events = (events_forced | sock_poll->events);

            custom_poll_f* poll_i = sock_poll->fd->custom_poll ? sock_poll->fd->custom_poll : be_waiting_for_event;
            enum poll_events revents = poll_i(sock_poll->fd, asked_events, sleep);

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
            register_t slept = syscall_cond_wait(msg_queue_poll != 0, sleep < 0 ? 0 : (register_t)sleep);
            if(sleep > 0) {
                sleep = slept > sleep ? 0 : (int)(sleep-slept);
            }
            goto restart;
        }

    } while(sleep);

    return any_event;
}

int socket_poll(poll_sock_t* socks, size_t nsocks, int timeout, enum poll_events* msg_queue_poll) {
    int ret;

    if(nsocks < 0) return 0;

    if(nsocks == 0 && !msg_queue_poll) return 0;

    if(!(ret = sockets_scan(socks, nsocks, msg_queue_poll, 0))) {
        if(timeout == 0) return 0;
        ret = sockets_scan(socks, nsocks, msg_queue_poll, timeout);
    }

    return ret;
}

int assign_socket_n(unix_like_socket* sock) {
    if(sock->flags & SOCKF_GIVE_SOCK_N) return -1;
    sock->flags |= SOCKF_GIVE_SOCK_N;
    sock->sockn = alloc_sockn();
}

requester_t socket_malloc_requester_32(uint8_t socket_type, data_ring_buffer *paired_drb) {
    res_t res = cap_malloc(SIZE_OF_request(32));
    ERROR_T(requester_t) requester = socket_new_requester(res, 32, socket_type, paired_drb);
    return IS_VALID(requester) ? requester.val : NULL;
}

fulfiller_t socket_malloc_fulfiller(uint8_t socket_type) {
    res_t res = cap_malloc(SIZE_OF_fulfill);
    ERROR_T(fulfiller_t) fulfiller = socket_new_fulfiller(res, socket_type);
    return IS_VALID(fulfiller) ? fulfiller.val : NULL;
}