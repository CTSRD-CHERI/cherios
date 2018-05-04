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

/*
 * typedef struct ethII_frame {
    char dest_mac[6];
    char src_mac[6];
    u16  length;
    char payload[];
} ethII_frame;

typedef struct ethII_check {
    char sum[4];
} ethII_check;

typedef struct iphdr {
    u8    version:4, ihl:4;
    u8    tos;
    u16   tot_len;
    u16   id;
    u16   frag_off;
    u8    ttl;
    u8    protocol;
    u16   check;
    u32   saddr;
    u32   daddr;
    char  payload[];
} iphdr;
 *    printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\nStatus %x\nMax pairs: %d\n",
           config.mac[0], config.mac[1], config.mac[2], config.mac[3], config.mac[4], config.mac[5],
            config.status,
            config.max_virtqueue_pairs);

	// Send a simple packet
	le16 head, tail;
	head = tail = virtio_q_alloc(&virtq_recv, &free_head);

    // sign up for interrupts

    result = syscall_interrupt_register(VIRTIO_MMIO_NET_IRQ, act_self_ctrl, 0, 0, (capability)mmio);
    assert_int_ex(result, == , 0);
    result = syscall_interrupt_enable(VIRTIO_MMIO_NET_IRQ, act_self_ctrl);
    assert_int_ex(result, ==, 0);

    // Give a single descriptor to recieve...
	char* packet_cap = (char*)GET_A_PAGE;
    uint64_t packet_phy = mem_paddr_for_vaddr((size_t)packet_cap);
    volatile struct virtio_net_hdr* packet_hdr = (struct virtio_net_hdr*)packet_cap;

    virtq_recv.desc[head].addr = packet_phy;
    virtq_recv.desc[head].len = MEM_REQUEST_MIN_REQUEST;
    virtq_recv.desc[head].flags = VIRTQ_DESC_F_WRITE;

    while(1) {
        printf("Recv packet at %lx\n", packet_phy);
        virtio_q_add_descs(&virtq_recv, head);
        virtio_device_notify(mmio, 0);

        // Wait for message

        msg_t* msg = get_message();

        printf("Got interrupt!\n");

        assert(msg->c3 == (capability)mmio);


        printf("Used? %x. Split into: %x\n",virtq_recv.used->idx, packet_hdr->num_buffers);

        next_msg();

        virtio_device_ack_used(mmio);
        syscall_interrupt_enable(VIRTIO_MMIO_NET_IRQ, act_self_ctrl);
    }

	return 0;
 *
 *
	struct virtio_net_hdr* packet_hdr = (struct virtio_net_hdr*)packet_cap;
    ethII_frame* eth_packet = (ethII_frame*)((char*)packet_cap + sizeof(struct virtio_net_hdr));

	packet_hdr->flags = 0;
	packet_hdr->gso_type = 0;
	packet_hdr->hdr_len = 0;
	packet_hdr->gso_size = 0;
	packet_hdr->csum_offset = 0;
	packet_hdr->csum_start = 0;
	packet_hdr->num_buffers = 0;


    eth_packet->dest_mac[0] = (char)0xDE;
    eth_packet->dest_mac[1] = (char)0xAD;
    eth_packet->dest_mac[2] = (char)0xBE;
    eth_packet->dest_mac[3] = (char)0xEF;
    eth_packet->dest_mac[4] = (char)0xCA;
    eth_packet->dest_mac[5] = (char)0xFE;


    eth_packet->length = 0x0800;
    memcpy(&eth_packet->src_mac, &config.mac, 6);

    virtq_send.desc[head].addr = packet_phy;
    virtq_send.desc[head].len = (le32)(sizeof(struct virtio_net_hdr) + sizeof(ethII_frame));
    virtq_send.desc[head].flags = VIRTQ_DESC_F_NEXT;
    packet_phy += (le32)(sizeof(struct virtio_net_hdr) + sizeof(ethII_frame)) + 2; // get alignment back on track;
    packet_cap += (le32)(sizeof(struct virtio_net_hdr) + sizeof(ethII_frame)) + 2; // get alignment back on track;

#define MES "Hello World!\n"
    u16 ip_length = sizeof(MES) + sizeof(iphdr);
    iphdr* ip_packet = (iphdr*)(packet_cap);

    ip_packet->version = 4;
    ip_packet->ihl = 5; // no options
    ip_packet->tos = 0;
    ip_packet->tot_len = ip_length;
    ip_packet->id = 0;
    ip_packet->frag_off = 0;
    ip_packet->ttl = 0x10;
    ip_packet->protocol = 0x6; // TCP
    ip_packet->check = 0; // checksum TODO
    ip_packet->saddr = 0x1234;
    ip_packet->daddr = 0x5678;
    memcpy(ip_packet->payload, MES, sizeof(MES));

    ethII_check* chk = (ethII_check*)(((char*)(eth_packet)) + ip_length);

    chk->sum[0] = chk->sum[1] = chk->sum[2] = chk->sum[3] = (char)0xAB;

    uint64_t part2 = ip_length + sizeof(ethII_check);

    virito_q_chain_add(&virtq_send, &free_head, &tail, packet_phy, (le16)part2, 0);
 */