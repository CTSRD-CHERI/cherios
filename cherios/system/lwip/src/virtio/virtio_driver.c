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

#include "lwip_driver.h"
#include "mman.h"
#include "malta_virtio_mmio.h"

static void alloc_recv(net_session* session) {

    // FIXME these buffers are too large for most packets. Better to have smaller ones and chain them
    // enough for a least 1 more recieve chain
    while (session->recvs_free >= 2) {
        // We create a custom pbuf here thats is malloc'd. We do this because they _may_ be passed to userspace if TCP
        custom_for_tcp* custom = alloc_custom(session);

        struct pbuf* pb = &custom->as_pbuf.custom.pbuf;


        le16 head = virtio_q_alloc(&session->virtq_recv, &session->free_head_recv);



        assert_int_ex(head, !=, session->virtq_recv.num);

        session->pbuf_recv_map[head] = pb;

        le16 tail = head;


        struct virtq_desc* desc = session->virtq_recv.desc+head;

        desc->addr = NET_HDR_P(head);
        desc->len = sizeof(struct virtio_net_hdr);
        desc->flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;

        int ret = virtio_q_chain_add(&session->virtq_recv, &session->free_head_recv, &tail,
                                     ((size_t)pb->payload) + ETH_PAD_SIZE + custom->offset, pb->len - ETH_PAD_SIZE,
                                     VIRTQ_DESC_F_WRITE);

        assert_int_ex(ret, ==, 0);

        virtio_q_add_descs(&session->virtq_recv, (le16)head);

        session->recvs_free -= (2);
    }

    virtio_device_notify(session->mmio, 0);
}

static void free_send(net_session* session) {
    struct virtq* sendq = &session->virtq_send;

    // First free any pbufs/descs on the out path that have finished
    while(sendq->last_used_idx != sendq->used->idx) {
        // Free pbufs that have been used
        le16 used_idx = (le16)(sendq->last_used_idx & (sendq->num-1));
        struct virtq_used_elem used = sendq->used->ring[used_idx];

        struct pbuf* pb = session->pbuf_send_map[used.id];
        //struct virtio_net_hdr* net_hdr = &session->net_hdrs[SEND_HDR_START + (used.id)];

        pbuf_free(pb);

        virtio_q_free_chain(sendq, &session->free_head_send, (le16)used.id);

        sendq->last_used_idx++;
    }
}

int lwip_driver_init(net_session* session) {

    _safe cap_pair pair;

    // Get MMIO
    get_physical_capability(VIRTIO_MMIO_NET_BASE, VIRTIO_MMIO_SIZE, 1, 0, own_mop, &pair);

    session->mmio = (lwip_driver_mmio_t*)pair.data;

    // Setup queues
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
    assert_int_ex(MEM_REQUEST_MIN_REQUEST, >, sizeof(struct virtio_net_hdr) * ((QUEUE_SIZE * 2)));
    session->net_hdrs_paddr = translate_address((size_t)session->net_hdrs, 0);

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
    session->recvs_free = QUEUE_SIZE;

    session->config = *(volatile struct virtio_net_config*)session->mmio->config;

    alloc_recv(session);

    session->irq = VIRTIO_MMIO_NET_IRQ;
    syscall_interrupt_register(session->irq, act_self_ctrl, -1, 0, session);

    return 0;
}

int lwip_driver_init_postup(net_session* session) {
    virtio_device_ack_used(session->mmio);
    syscall_interrupt_enable(session->irq, act_self_ctrl);
    return 0;
}

void lwip_driver_handle_interrupt(net_session* session, __unused register_t arg, register_t irq) {
    free_send(session);

    // Then process incoming packets and pass them up to lwip
    struct virtq* recvq = &session->virtq_recv;
    int any_in = 0;
    while(recvq->last_used_idx != recvq->used->idx) {
        any_in = 1;

        size_t used_idx = recvq->last_used_idx & (recvq->num-1);

        struct virtq_used_elem used = recvq->used->ring[used_idx];

        struct pbuf* pb = session->pbuf_recv_map[used.id];
        struct virtio_net_hdr* net_hdr = &session->net_hdrs[used.id];

        assert_int_ex(net_hdr->num_buffers, ==, 1); // Otherwise we will have to gather and I CBA

        // NOTE: input will free its pbuf!
        session->nif->input(pb, session->nif);

        int freed = virtio_q_free_chain(recvq, &session->free_head_recv, used.id);
        session->recvs_free += freed;
        recvq->last_used_idx++;
    }

    if(any_in) alloc_recv(session);
    virtio_device_ack_used(session->mmio);

    // Renable interrupts
    syscall_interrupt_enable((int)irq, act_self_ctrl);
}

err_t lwip_driver_output(struct netif *netif, struct pbuf *p) {
    net_session* session = netif->state;

    // Try reclaim as many send buffers as possible
    free_send(session);

    struct virtq* sendq = &session->virtq_send;

    le16 head = virtio_q_alloc(sendq, &session->free_head_send);

    if(head == QUEUE_SIZE) {
        return ERR_MEM;
    }

    le16 tail = head;

    struct virtq_desc* desc = sendq->desc + head;

    struct virtio_net_hdr* net_hdr = &session->net_hdrs[SEND_HDR_START + (head)];

    bzero(net_hdr, sizeof(struct virtio_net_hdr));

    desc->addr = NET_HDR_P(SEND_HDR_START + (head));
    desc->len = sizeof(struct virtio_net_hdr);
    desc->flags = VIRTQ_DESC_F_NEXT;


    struct pbuf* p_head = p;

    int first_pbuf = 1;
    do {
        capability payload = (capability)(((char*)p->payload));
        capability extra_payload = p->sealed_payload;

        if(extra_payload) {
            extra_payload = cheri_unseal(extra_payload, ether_sealer);
            if(!cheri_gettag(payload)) {
                size_t diff = (char*)payload - (char*)extra_payload;
                payload = (char*)extra_payload + diff;
            }
        }

        le32 size = (le32)(p->len);
        if(first_pbuf) {
            payload = (char*)payload + ETH_PAD_SIZE;
            size -=ETH_PAD_SIZE;
            first_pbuf = 0;
        }

        int res = 0;

        if(p->sp_length == 0) {
           res = virtio_q_chain_add_virtual(sendq, &session->free_head_send, &tail, payload, size, VIRTQ_DESC_F_NEXT);
        } else {

            uint16_t dst_off = p->sp_dst_offset;
            uint16_t src_off = p->sp_src_offset;
            uint16_t extra_len = p->sp_length;

            if(dst_off) {
                res = virtio_q_chain_add_virtual(sendq, &session->free_head_send, &tail, payload, dst_off, VIRTQ_DESC_F_NEXT);
            }
            if(res >= 0) {
                res = virtio_q_chain_add_virtual(sendq, &session->free_head_send, &tail, (char*)extra_payload + src_off, extra_len, VIRTQ_DESC_F_NEXT);

                uint32_t so_far = dst_off + extra_len;
                int32_t remain = size - so_far;
                if(res >= 0 && (so_far < size)) {
                    res = virtio_q_chain_add_virtual(sendq, &session->free_head_send, &tail, (char*)payload + so_far, remain, VIRTQ_DESC_F_NEXT);
                }
            }
        }

        if(res < 0) {
            // We are out of buffers =(
            virtio_q_free(sendq, &session->free_head_send, head, tail);
            return ERR_MEM;
        }
    } while(p->len != p->tot_len && (p = p->next));

    sendq->desc[tail].flags = 0;

    // Put in a reference until this send has finished

    session->pbuf_send_map[head] = p_head;
    //CHERI_PRINT_CAP(p_head);
    pbuf_ref(p_head); // We only need to put in a ref on the head. Do so only after we know this write is succeeding

    virtio_q_add_descs(sendq, (le16)head);

    // Notify device there are packets to send
    virtio_device_notify(session->mmio, 1);

    return ERR_OK;
}

int lwip_driver_poll(net_session* session) {
    return
            (session->virtq_send.last_used_idx != session->virtq_send.used->idx) ||
            (session->virtq_recv.last_used_idx != session->virtq_recv.used->idx);
}