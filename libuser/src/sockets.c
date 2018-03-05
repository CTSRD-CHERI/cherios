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

#include "object.h"
#include "string.h"
#include "sockets.h"
#include "stdio.h"

static int is_empty(uni_dir_socket_writer* writer) {
    return (writer->reader_component.read_ptr == writer->write_ptr);
}

static int is_full(uni_dir_socket_writer* writer) {
    return ((writer->write_ptr - writer->reader_component.read_ptr) == writer->buffer_size);
}

static uint16_t fill_level(uni_dir_socket_writer* writer) {
    return (writer->write_ptr - writer->reader_component.read_ptr);
}

static uint16_t space(uni_dir_socket_writer* writer) {
    return (writer->buffer_size - (writer->write_ptr - writer->reader_component.read_ptr));
}

static uint16_t space_from_read_ptr(uni_dir_socket_writer* writer, uint16_t read_ptr) {
    return (writer->buffer_size - (writer->write_ptr - read_ptr));
}

static size_t data_buf_space(data_ring_buffer* data_buffer) {
    return (data_buffer->buffer_size - (data_buffer->write_ptr - data_buffer->read_ptr));
}

static int socket_internal_write_space_wait(uni_dir_socket_writer* writer, uint16_t need_space, int dont_wait) {

    if(writer->reader_component.reader_closed || writer->writer_closed) {
        return E_SOCKET_CLOSED;
    }

    int full = space(writer) <= need_space;

    if(!full) return 0;

    if(full && dont_wait) return E_AGAIN;

    volatile uint16_t* read_ptr_cap = &(writer->reader_component.read_ptr);
    volatile act_kt* writer_waiting_cap = &writer->reader_component.writer_waiting;
    uint16_t write_ptr = writer->write_ptr;
    uint16_t check = write_ptr + need_space - (writer->buffer_size);

    while(full) {
        // Gives 1 if full (and sets waiting), 2 if closed, 0 otherwise
        __asm__ __volatile(
        SANE_ASM
        "2:      cllc    $c1, %[waiting_cap]        \n"     // Load linked waiting
                "clb    $at, $zero, 0(%[cc])        \n"
                "bnez   $at, 1f                     \n"     // Check reader didn't close the socket
                "li     %[res], 2                   \n"
                "clh    $at, $zero, 0(%[read_ptr_cap])\n"   // Load read ptr
                "sltu   $at, %[check_val], $at      \n"     // 1 if not full
                "bnez   $at, 1f                     \n"     // If not full go to next step
                "li     %[res], 0                   \n"
                "cscc   $at, %[self], %[waiting_cap] \n" // Try set in waiting state
                "beqz   $at, 2b                     \n"     // If we failed to write the cap, try all this again
                "li     %[res], 1                   \n"
                "1:                                 \n"
        : [res]"=r"(full)
        : [waiting_cap]"C"(writer_waiting_cap),
        [self]"C"(act_self_notify_ref),
        [read_ptr_cap]"C"(read_ptr_cap),
        [check_val]"r"(check),
        [cc]"C"(&writer->reader_component.reader_closed)
        : "$c1", "at"
        );

        if(full == 2) {
            return E_SOCKET_CLOSED;
        }
        if(full) syscall_cond_wait();
    }

    return 0;
}

static int socket_internal_write(uni_dir_socket_writer* writer, char* buffer, int dont_wait) {

    socket_internal_write_space_wait(writer, 1, dont_wait);

    uint16_t write_ptr = writer->write_ptr;

    // Put a cap in the buffer
    writer->indirect_ring_buffer[write_ptr & (writer->buffer_size-1)] = buffer;


    uint16_t write_ptr_new = (write_ptr + 1);

    // Updates write_ptr. Will cause reader to fail setting waiting on race. Returns the waiter (if any)
    act_kt waiter;
    __asm__ __volatile(
            "cllc   %[res], %[waiting_cap]                     \n"
            "csh    %[new_write], $zero, 0(%[new_write_cap])   \n"
            "cscc   $at, %[res], %[waiting_cap]                \n"
            "clc    %[res], $zero, 0(%[waiting_cap])           \n"
    : [res]"=C"(waiter)
    : [waiting_cap]"C"(&writer->reader_component.reader_waiting),
    [new_write_cap]"C"(&writer->write_ptr),
    [new_write]"r"(write_ptr_new)
    : "at"
    );

    if(waiter) {
        writer->reader_component.reader_waiting = NULL;
        syscall_cond_notify(waiter);
    }

    return 0;
}

static int socket_internal_read_peek(uni_dir_socket_reader* reader, uint16_t amount, int dont_wait) {
    uni_dir_socket_writer_reader_component* access = reader->writer->access;

    if(reader->writer->writer_closed || access->reader_closed) {
        return E_SOCKET_CLOSED;
    }

    int empty = fill_level(reader->writer) < amount;

    if(!empty) return 0;

    if(dont_wait) return E_AGAIN;

    volatile act_kt* reader_waiting_cap = &access->reader_waiting;
    volatile uint16_t* write_ptr_cap = &(reader->writer->write_ptr);
    uint16_t check = amount + access->read_ptr;

    while(empty) {
        // Gives 1 if not full enough and sets waiting
        __asm__ __volatile(
        SANE_ASM
        "2:      cllc   $c1, %[waiting_cap]                 \n" // Load linked waiting
                "clb    $at, $zero, 0(%[cc])                \n"
                "bnez   $at, 1f                             \n" // Check writer didn't close the socket
                "li     %[res], 2                           \n"
                "clh    %[res], $zero, 0(%[write_ptr_cap])  \n" // Load read ptr
                "sltu   %[res], %[res], %[check_val]        \n" // Check if there is enough stuff
                "beqz   %[res], 1f                          \n" // If enough go to next step
                "nop                                        \n"
                "cscc   %[res], %[self], %[waiting_cap]     \n"  // Try set in waiting state
                "beqz   %[res], 2b                          \n"     // If we failed to write the cap, try all this again
                "li     %[res], 1                           \n"
        "1:\n"
        : [res]"=r"(empty)
        : [waiting_cap]"C"(reader_waiting_cap),
        [self]"C"(act_self_notify_ref),
        [write_ptr_cap]"C"(write_ptr_cap),
        [check_val]"r"(check),
        [cc]"C"(&reader->writer->writer_closed)
        : "$c1"
        );

        if(empty == 2) {
            return E_SOCKET_CLOSED;
        }

        if(empty) syscall_cond_wait();
    }

    return 0;
}

// Progresses read_ptr. Call peek FIRST.

static int socket_internal_read_progress(uni_dir_socket_reader* reader, uint16_t amount, int dont_wait) {

    uni_dir_socket_writer_reader_component* access = reader->writer->access;

    uint16_t read_ptr_new = access->read_ptr + amount;

    // Updates read_ptr. Will cause writer to fail setting waiting on race. Returns the waiter (if any)
    act_kt waiter;
    __asm__ __volatile(
        "cllc   %[res], %[waiting_cap]                     \n"
        "csh    %[new_read], $zero, 0(%[new_read_cap])     \n"
        "cscc   $at, %[res], %[waiting_cap]                \n"
        "clc    %[res], $zero, 0(%[waiting_cap])           \n"
    : [res]"=C"(waiter)
    : [waiting_cap]"C"(&access->writer_waiting),
      [new_read_cap]"C"(&access->read_ptr),
      [new_read]"r"(read_ptr_new)
    : "at"
    );

    if(waiter) {
        access->writer_waiting = NULL;
        syscall_cond_notify(waiter);
    }

    return 0;
}

// Brings the data buffer read pointer back in line.
static uint16_t socket_internal_data_buffer_reclaim(data_ring_buffer* data_buffer, uni_dir_socket_writer* writer) {

    // TODO need a sync here?
    uint16_t process_to = writer->reader_component.read_ptr;

    if(data_buffer->last_processed_read_ptr != process_to) {
        // We need to reclaim up to this capability

        uint16_t last_read = (process_to - 1) & (writer->buffer_size-1);

        volatile char* last_read_cap = writer->indirect_ring_buffer[last_read];

        size_t mask = data_buffer->buffer_size-1;

        size_t ends_at = (cheri_getbase(last_read_cap) + cheri_getlen(last_read_cap)) - cheri_getbase(data_buffer->buffer);

        size_t old_read_ptr = data_buffer->read_ptr;
        size_t new_read_ptr = (old_read_ptr & ~mask) | ends_at;

        if(new_read_ptr <= old_read_ptr) new_read_ptr+= data_buffer->buffer_size;

        data_buffer->read_ptr = new_read_ptr;
        data_buffer->last_processed_read_ptr = process_to;
    }

    return process_to;
}

static ssize_t socket_internal_data_buffer_write(const char* buf, size_t size,
                                                 data_ring_buffer* data_buffer, uni_dir_socket_writer* writer,
                                                 int dont_wait) {
    int res;

    size_t mask = data_buffer->buffer_size-1;

    size_t copy_from = (data_buffer->write_ptr) & mask;
    size_t part_1 = data_buffer->buffer_size - copy_from;

    int two_parts = part_1 < size;

    res = socket_internal_write_space_wait(writer, two_parts ? 2 : 1, dont_wait);

    if(res < 0 ) return res;

    // The two write buffers keep falling out of sync. Bring back in line.
    uint16_t proc_to = socket_internal_data_buffer_reclaim(data_buffer, writer);

    size_t data_space = data_buf_space(data_buffer);

    // Can't write this message because the data buffer does not have enough space
    if(data_space < size && dont_wait) return E_AGAIN;

    // Reclaim data buffer space until there is enough to copy
    uint16_t write_ptr = writer->write_ptr;
    while (data_space <= size) {
        uint16_t read_ptr = writer->reader_component.read_ptr;

        if(read_ptr == proc_to) {
            uint16_t proc_space = write_ptr - read_ptr;
            // Wait for space to increase by at least 1
            res = socket_internal_write_space_wait(writer, proc_space+1, dont_wait);

            if(res < 0) return res;
        }

        proc_to = socket_internal_data_buffer_reclaim(data_buffer, writer);
        data_space = data_buf_space(data_buffer);
    }

    // We are okay to make a write. First copy to buffer and then send those bytes.
    // TODO we could allow socket sending of capabilities / faster memcpy if we ensured alignment in the ring buffer
    part_1 = two_parts ? part_1 : size;

    memcpy(data_buffer->buffer + copy_from, buf, part_1);
    char* cap1 = cheri_setbounds(data_buffer->buffer + copy_from, part_1);
    cap1 = cheri_andperm(cap1, CHERI_PERM_LOAD);
    res = socket_internal_write(writer, cap1, dont_wait);
    if(res < 0) return res;
    data_buffer->write_ptr+=part_1;

    if(two_parts) {
        size_t part_2 = size - part_1;
        memcpy(data_buffer->buffer, buf + part_1, part_2);
        char* cap2 = cheri_setbounds(data_buffer->buffer, part_2);
        cap2 = cheri_andperm(cap2, CHERI_PERM_LOAD);
        res = socket_internal_write(writer, cap2, dont_wait);
        if(res < 0) return res;
        data_buffer->write_ptr+=part_2;
    }

    return size;
}

int socket_internal_init(bi_dir_socket* sock, uint16_t buffer_size) {
    if(!is_power_2(buffer_size)) return E_BUFFER_SIZE_NOT_POWER_2;

    bzero(sock, sizeof(bi_dir_socket));
    sock->writer.buffer_size = buffer_size;
    uni_dir_socket_writer_reader_component* access = &(sock->writer.reader_component);
    access = cheri_setbounds(access, sizeof(uni_dir_socket_writer_reader_component));
    sock->writer.access = access;

    return 0;
}

int socket_internal_listen(register_t port, bi_dir_socket* sock) {
    msg_t msg;
    pop_msg(&msg);

    if(msg.c5 == NULL) return (E_CONNECT_FAIL);

    if(msg.v0 != port || msg.c3 == NULL) {
        // Send failure
        message_reply((capability)E_CONNECT_FAIL,0,0,msg.c2, msg.c1);
        return (E_CONNECT_FAIL);
    }

    // Get socket struct from connector
    act_kt sender = msg.c5;
    sock->reader.writer = (uni_dir_socket_writer*) msg.c3;

    // Construct our own socket struct
    write_ptr_t read_only_writer = (write_ptr_t)cheri_andperm(&(sock->writer), CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);

    // ACK receipt
    message_reply((capability)read_only_writer,0,0,msg.c2, msg.c1);

    // TODO should we add an extra ack?

    return 0;
}

int socket_internal_connect(act_kt target, register_t port, bi_dir_socket* sock) {
    msg_t msg;

    // Construct our own socket struct
    capability read_only_writer = (capability)cheri_andperm(&(sock->writer), CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
    // Send socket details

    capability cap_result = message_send_c(0,0,0,0,read_only_writer, NULL, act_self_ref, NULL, target, SYNC_CALL, port);

    ERROR_T(write_ptr_t) res = ER_T_FROM_CAP(write_ptr_t, cap_result);

    // Check ACK
    if(!IS_VALID(res)) return (int)res.er;

    sock->reader.writer = (uni_dir_socket_writer*) res.val;

    return 0;
}

static int socket_internal_close_safe(volatile uint8_t* own_close, volatile uint8_t* other_close, volatile act_notify_kt * waiter_cap) {
    if(*own_close) return E_ALREADY_CLOSED;

    // We need to signal the other end

    *own_close = 1;

    if(*other_close) {
        return 0; // If the other has closed on their end they won't expect a signal
    }

    // Otherwise the reader may be about to sleep / already be asleep.

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

int socket_internal_close_writer(uni_dir_socket_writer* writer) {
    return socket_internal_close_safe(&writer->writer_closed,
                                      &writer->reader_component.reader_closed,
                                      &writer->reader_component.reader_waiting);
}

int socket_internal_close_reader(uni_dir_socket_reader* reader) {
    uni_dir_socket_writer_reader_component* access = reader->writer->access;
    return socket_internal_close_safe(&access->reader_closed,
                                      &reader->writer->writer_closed,
                                      &access->writer_waiting);
}

int socket_internal_close(bi_dir_socket* sock) {
    int res;
    res = socket_internal_close_reader(&sock->reader);
    int res2;
    res2 = socket_internal_close_writer(&sock->writer);

    return (res | res2);
}

int socket_init(unix_like_socket* sock, enum SOCKET_FLAGS flags, char* data_buffer, size_t data_buffer_size, uint16_t internal_buffer_size) {
    if(!is_power_2(data_buffer_size)) return E_BUFFER_SIZE_NOT_POWER_2;

    int res = socket_internal_init(&sock->socket, internal_buffer_size);
    if(res < 0) return res;

    sock->flags = flags;

    sock->copy_buffer.buffer_size = data_buffer_size;
    sock->copy_buffer.buffer = cheri_setbounds(data_buffer, data_buffer_size);
    sock->copy_buffer.read_ptr = 0;
    sock->copy_buffer.write_ptr = 0;
    sock->copy_buffer.last_processed_read_ptr = 0;

    return 0;
}

ssize_t socket_send(unix_like_socket* sock, const char* buf, size_t length, enum SOCKET_FLAGS flags) {
    // Need to copy to buffer in order not to expose buf

    if(length == 0) return 0;

    if(length > sock->copy_buffer.buffer_size) return E_MSG_SIZE;

    uni_dir_socket_writer* writer = &sock->socket.writer;

    return socket_internal_data_buffer_write(buf, length,
                                      &sock->copy_buffer, writer,
                                             (sock->flags | flags) & MSG_DONT_WAIT);
}

ssize_t socket_recv(unix_like_socket* sock, char* buf, size_t length, enum SOCKET_FLAGS flags) {

    int res;
    uni_dir_socket_writer* writer = sock->socket.reader.writer;
    uint16_t mask = writer->buffer_size - 1;
    uint16_t read_ptr = writer->reader_component.read_ptr;

    if(writer->writer_closed || writer->reader_component.reader_closed) {
        return E_SOCKET_CLOSED;
    }

    if(length == 0) return 0;

    size_t bytes_left = length;



    size_t partial_bytes = sock->socket.reader.partial_read_bytes;

    while(bytes_left != 0) {

        // If this is non zero then we will have at least one capability usable
        if(partial_bytes == 0) {
            res = socket_internal_read_peek(&sock->socket.reader, 1, (sock->flags | flags) & MSG_DONT_WAIT);
            if(res < 0) break;
        }

        char* cap = writer->indirect_ring_buffer[read_ptr & mask];
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

        memcpy(buf + length - bytes_left, cap + partial_bytes, to_copy);
        bytes_left -= to_copy;
        partial_bytes = new_partial;

        if(partial_bytes == 0) {
            read_ptr++;
            // TODO we really could progress multiple caps at a time, but for a large request this might fill up the buffer
            res = socket_internal_read_progress(&sock->socket.reader, 1, (sock->flags | flags) & MSG_DONT_WAIT);
            if(res < 0) break;
        }
    }

    sock->socket.reader.partial_read_bytes = partial_bytes;
    ssize_t actually_read = length - bytes_left;


    // FIXME: Need to somehow convey res as we may have had an error
    if(actually_read == 0) return res;
    return actually_read;
}
