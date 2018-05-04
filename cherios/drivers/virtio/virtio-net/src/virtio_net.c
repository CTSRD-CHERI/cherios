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

#include "sockets.h"
#include "stdlib.h"
#include "object.h"
#include "stdio.h"
#include "virtio.h"
#include "virtio_net.h"
#include "misc.h"
#include "malta_virtio_mmio.h"
#include "assert.h"
#include "mman.h"
#include "string.h"
#include "thread.h"

#define QUEUE_SIZE 0x10

typedef struct net_session {
    virtio_mmio_map* mmio;
    uint8_t irq;
    struct virtq virtq_send, virtq_recv;
    le16 free_head_send;
    le16 free_head_recv;
    struct virtio_net_config config;
    uni_dir_socket_fulfiller transmit_fulfiller;
    struct requester_32 receive_requester;
} net_session;
typedef capability net_session_sealed;






capability net_session_sealer;






void virtio_daemon_start(void) {
    net_session_sealer = get_type_owned_by_process();

    // Handle loop:
    while(1) {
        if(!msg_queue_empty()) {
            msg_entry(1);
        }
        wait();
    }
}


net_session_sealed seal_session(net_session* session) {
    return (net_session_sealed)cheri_seal(session, net_session_sealer);
}

net_session* unseal_session(net_session_sealed sealed) {
    return (net_session*)cheri_unseal(sealed, net_session_sealer);
}

int netsession_attach_sockets(net_session_sealed ss) {
    net_session* session = unseal_session(ss);
}

net_session_sealed netsession_create(virtio_mmio_map* mmio, uint8_t irq) {
    assert(mmio != NULL);
    // First init

    net_session* session = (net_session*)malloc(sizeof(net_session));
    session->mmio = mmio;
    session->irq = irq;

    // Init socket layer
    socket_internal_requester_init(&session->receive_requester.r, 32, SOCK_TYPE_PUSH, NULL);
    socket_internal_fulfiller_init(&session->transmit_fulfiller, SOCK_TYPE_PUSH);

    // Setup queues
    cap_pair pair;
#define GET_A_PAGE (rescap_take(mem_request(0, MEM_REQUEST_MIN_REQUEST, NONE, own_mop).val, &pair), pair.data)

    session->virtq_send.num = QUEUE_SIZE;
    session->virtq_send.avail = (struct virtq_avail*)GET_A_PAGE;
    session->virtq_send.desc = (struct virtq_desc*)GET_A_PAGE;
    session->virtq_send.used = (struct virtq_used*)GET_A_PAGE;

    session->virtq_recv.num = QUEUE_SIZE;
    session->virtq_recv.avail = (struct virtq_avail*)GET_A_PAGE;
    session->virtq_recv.desc = (struct virtq_desc*)GET_A_PAGE;
    session->virtq_recv.used = (struct virtq_used*)GET_A_PAGE;

    u32 features = (1 << VIRTIO_NET_F_MRG_RXBUF);
    int result = virtio_device_init(mmio, net, 1, VIRTIO_QEMU_VENDOR, features);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_queue_add(mmio, 0, &session->virtq_recv);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_queue_add(mmio, 1, &session->virtq_send);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_device_ready(mmio);
    assert_int_ex(-result, ==, 0);

    virtio_q_init_free(&session->virtq_recv, &session->free_head_recv, 0);
    virtio_q_init_free(&session->virtq_send, &session->free_head_send, 0);

    session->config = *(struct virtio_net_config*)mmio->config;

    net_session_sealed ss = seal_session(session);
    syscall_interrupt_register(irq, act_self_ctrl, -1, 0, ss);
    syscall_interrupt_enable(session->irq, act_self_ctrl);

    return ss;
}

void interrupt(net_session_sealed* ss) {
    net_session* session = unseal_session(ss);

    // TODO a thing with the interrupt

    syscall_interrupt_enable(session->irq, act_self_ctrl);
}

void (*msg_methods[]) = {netsession_create};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);