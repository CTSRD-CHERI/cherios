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


#include "cheric.h"
#include "virtio_queue.h"
#include "virtio_net.h"
#include "virtio.h"
#include "sockets.h"
#include "stdlib.h"
#include "malta_virtio_mmio.h"
#include "assert.h"
#include "mman.h"
#include "misc.h"
#include "syscalls.h"

#include "lwip/inet.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/init.h"
#include "lwip/stats.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip4_frag.h"

// This driver is a stepping stone to get lwip working. It will eventually be extracted into into the stand-alone driver
// and they will communicate via the socket API.

#define QUEUE_SIZE 0x10

typedef struct net_session {
    virtio_mmio_map* mmio;
    struct netif* nif;
    uint8_t irq;
    struct virtq virtq_send, virtq_recv;

    // Net headers for send and recieve (first half for recv, second half for send)
#define NET_HDR_P(N) session->net_hdrs_paddr + (sizeof(struct virtio_net_hdr) * (N))
#define SEND_HDR_START (QUEUE_SIZE/2)

    struct virtio_net_hdr* net_hdrs;
    size_t net_hdrs_paddr;
    struct pbuf* pbuf_recv_map[QUEUE_SIZE/2];
    struct pbuf* pbuf_send_map[QUEUE_SIZE/2];

    le16 free_head_send;
    le16 free_head_recv;
    struct virtio_net_config config;

    ip4_addr_t gw_addr, my_ip, netmask;
    register_t ts_etharp, ts_tcp, ts_ipreass;
} net_session;

err_t netsession_init(struct netif* nif) {
    assert(nif != NULL);

    // First init

    net_session* session = nif->state;
    session->nif = nif;

    // Setup queues
    cap_pair pair;
#define GET_A_PAGE (rescap_take(mem_request(0, MEM_REQUEST_MIN_REQUEST, NONE, own_mop).val, &pair), pair.data)

    assert(is_power_2(QUEUE_SIZE));

    session->virtq_send.num = QUEUE_SIZE;
    session->virtq_send.avail = (struct virtq_avail*)GET_A_PAGE;
    session->virtq_send.desc = (struct virtq_desc*)GET_A_PAGE;
    session->virtq_send.used = (struct virtq_used*)GET_A_PAGE;

    session->virtq_recv.num = QUEUE_SIZE;
    session->virtq_recv.avail = (struct virtq_avail*)GET_A_PAGE;
    session->virtq_recv.desc = (struct virtq_desc*)GET_A_PAGE;
    session->virtq_recv.used = (struct virtq_used*)GET_A_PAGE;

    session->net_hdrs = (struct virtio_net_hdr*)GET_A_PAGE;
    assert_int_ex(MEM_REQUEST_MIN_REQUEST, >, sizeof(struct virtio_net_hdr) * ((QUEUE_SIZE/2) * 2));
    session->net_hdrs_paddr = mem_paddr_for_vaddr((size_t)session->net_hdrs);

    u32 features = (1 << VIRTIO_NET_F_MRG_RXBUF);
    int result = virtio_device_init(session->mmio, net, 1, VIRTIO_QEMU_VENDOR, features);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_queue_add(session->mmio, 0, &session->virtq_recv);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_queue_add(session->mmio, 1, &session->virtq_send);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_device_ready(session->mmio);
    assert_int_ex(-result, ==, 0);

    virtio_q_init_free(&session->virtq_recv, &session->free_head_recv, 0);
    virtio_q_init_free(&session->virtq_send, &session->free_head_send, 0);

    session->config = *(struct virtio_net_config*)session->mmio->config;

    syscall_interrupt_register(session->irq, act_self_ctrl, -1, 0, nif);

    return 0;
}

err_t myif_link_output(struct netif *netif, struct pbuf *p);

// With a fudge factor for "clock" speed, sim speed, to convert to something like a millisecond
#define TMR_DIFF(A, B) (((A)-(B)) >> 15)
#define BC(c, l) \
    do { \
    size_t __start = mem_paddr_for_vaddr((size_t)(c));\
    size_t __end = mem_paddr_for_vaddr(((size_t)(c)) + l - 1);\
    assert_int_ex(__start + l - 1, ==, __end);              \
    } while(0)

int init_net(net_session* session, struct netif* nif) {
    lwip_init();


    nif = netif_add(nif, &session->my_ip, &session->netmask, &session->gw_addr,
                    (void*)session, &netsession_init, &ethernet_input);
    if(nif == NULL) return -1;

    memcpy(nif->hwaddr, session->config.mac, 6);
    nif->flags |= NETIF_FLAG_ETHARP;

    nif->linkoutput = &myif_link_output;
    nif->output = &etharp_output;

    netif_set_default(nif);
    netif_set_up(nif);

    for(size_t i = 0; i < QUEUE_SIZE/2; i++) {
        struct pbuf* pb = pbuf_alloc(PBUF_RAW, TCP_MSS + PBUF_TRANSPORT, PBUF_RAM);

        BC(pb->payload, pb->len);

        // struct virtio_net_hdr* net_hdr = &session->net_hdrs[i];
        // Allocate a buffer that can hold a tcp MSS plus a transport layer header
        session->pbuf_recv_map[i] = pb;

        le16 head = virtio_q_alloc(&session->virtq_recv, &session->free_head_recv);
        le16 tail = head;

        assert_int_ex(head / 2, ==, i);

        struct virtq_desc* desc = session->virtq_recv.desc+head;

        desc->addr = NET_HDR_P(i);
        desc->len = sizeof(struct virtio_net_hdr);
        desc->flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;

        // FIXME crossing page boundry check
        virtio_q_chain_add(&session->virtq_recv, &session->free_head_recv, &tail,
                           mem_paddr_for_vaddr((size_t)pb->payload)+ETH_PAD_SIZE, pb->len, VIRTQ_DESC_F_WRITE);

        assert_int_ex(head+1, ==, tail);
        virtio_q_add_descs(&session->virtq_recv, (le16)head);
    }

    // notify device the recv queue has been filled
    virtio_device_notify(session->mmio, 0);
    syscall_interrupt_enable(session->irq, act_self_ctrl);

    register_t now = syscall_now();

    session->ts_etharp = session->ts_ipreass = session->ts_tcp = now;
    return 0;
}

err_t netsession_timers(struct netif* nif) {

    net_session* session = nif->state;

    register_t now = syscall_now();

    if(TMR_DIFF(now, session->ts_etharp) > ARP_TMR_INTERVAL) {
        session->ts_etharp = now;
        etharp_tmr();
    }
    if(TMR_DIFF(now, session->ts_tcp) == 250) {
        session->ts_tcp = now;
        // TODO a tcp thing???
    }
    if(TMR_DIFF(now,session->ts_ipreass) > IP_TMR_INTERVAL) {
        session->ts_ipreass = now;
        ip_reass_tmr();
    }

    return 0;
}

err_t myif_link_output(struct netif *netif, struct pbuf *p) {

    net_session* session = netif->state;

    assert_int_ex(p->len, ==, p->tot_len); // else I have to gather and IB lazy =p.

    struct virtq* sendq = &session->virtq_send;

    le16 head = virtio_q_alloc(sendq, &session->free_head_send);

    assert(head != QUEUE_SIZE);

    le16 tail = head;
    // going to use heads lowers bits for inverse map
    assert_int_ex(head % 2, ==, 0);

    // Put in a reference until this send has finished
    pbuf_ref(p);
    session->pbuf_send_map[head/2] = p;

    struct virtq_desc* desc = sendq->desc + head;

    struct virtio_net_hdr* net_hdr = &session->net_hdrs[SEND_HDR_START + (head/2)];

    bzero(net_hdr, sizeof(struct virtio_net_hdr));

    desc->addr = NET_HDR_P(SEND_HDR_START + (head/2));
    desc->len = sizeof(struct virtio_net_hdr);
    desc->flags = VIRTQ_DESC_F_NEXT;

    // TODO crossing a page problem
    // TODO probably have to bump reference count on pbuf until this write finishes.

    BC(p->payload, p->len);

    size_t addr = mem_paddr_for_vaddr(((size_t)p->payload) + ETH_PAD_SIZE);
    virtio_q_chain_add(sendq, &session->free_head_send, &tail, addr, p->len-ETH_PAD_SIZE, 0);

    assert(tail != QUEUE_SIZE);

    virtio_q_add_descs(sendq, (le16)head);

    // Notify device there are packets to send
    virtio_device_notify(session->mmio, 1);
}

void interrupt(struct netif* nif) {

    net_session* session = nif->state;

    struct virtq* sendq = &session->virtq_send;

    // First free any pbufs/descs on the out path that have finished
    while(sendq->last_used_idx != sendq->used->idx) {
        // Free pbufs that have been used
        size_t used_idx = sendq->last_used_idx & (sendq->num-1);
        struct virtq_used_elem used = sendq->used->ring[used_idx];

        struct pbuf* pb = session->pbuf_send_map[used.id/2];
        struct virtio_net_hdr* net_hdr = &session->net_hdrs[SEND_HDR_START + (used.id/2)];

        pbuf_free(session->pbuf_send_map[used.id/2]);

        virtio_q_free(sendq, &session->free_head_send, used.id, used.id+1);

        sendq->last_used_idx++;
    }

    // Then process incoming packets and pass them up to lwip
    struct virtq* recvq = &session->virtq_recv;
    int any_in = 0;
    while(recvq->last_used_idx != recvq->used->idx) {
        any_in = 1;

        size_t used_idx = recvq->last_used_idx & (recvq->num-1);

        struct virtq_used_elem used = recvq->used->ring[used_idx];

        assert(used.id % 2 == 0);

        struct pbuf* pb = session->pbuf_recv_map[used.id/2];
        struct virtio_net_hdr* net_hdr = &session->net_hdrs[used.id/2];

        assert_int_ex(net_hdr->num_buffers, ==, 1); // Otherwise we will have to gather and I CBA

        // NOTE: input will free its pbuf!
        nif->input(pb, nif);
        recvq->last_used_idx++;

        // pbuf_free(pb); // FIXME free when ready?

        // Allocate a new buffer in this slot
        pb = pbuf_alloc(PBUF_RAW, TCP_MSS + PBUF_TRANSPORT, PBUF_RAM);
        BC(pb->payload, pb->len);
        session->pbuf_recv_map[used.id/2] = pb;
        recvq->desc[used.id + 1].addr = mem_paddr_for_vaddr((size_t)pb->payload) + ETH_PAD_SIZE;

        virtio_q_add_descs(&session->virtq_recv, (le16)(used.id));
    }

    // TODO go through the send queue and say we have finished using the buffers
    if(any_in) virtio_device_notify(session->mmio, 0);
    virtio_device_ack_used(session->mmio);
    syscall_interrupt_enable(session->irq, act_self_ctrl);
}

int main(register_t arg, capability carg) {
    // Init session
    printf("LWIP Hello World!\n");

    net_session session;
    session.irq = (uint8_t)arg;
    session.mmio = (virtio_mmio_map*)carg;

    session.netmask.addr = 0xFFFFFF00;
    session.my_ip.addr = 0x0A000005;
    session.gw_addr.addr = 0x0A000001;

    struct netif nif;

    // Init LWIP (calls the rest of init session)
    int res = init_net(&session, &nif);

    assert_int_ex(res, ==, 0);

    // Main loop
    while(1) {
        netsession_timers(&nif);
        // Wait for a message or socket

        // Respond to messages (may include interrupt getting called)
        if(!msg_queue_empty()) {
            msg_entry(1);
        }

        // respond to sockets
        // TODO

        // sleep if we handled nothing on the last loop needs a duration of the minimum of our poll intervals
        // TODO
        wait();
    }
}

void (*msg_methods[]) = {};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, interrupt};
size_t ctrl_methods_nb = countof(ctrl_methods);