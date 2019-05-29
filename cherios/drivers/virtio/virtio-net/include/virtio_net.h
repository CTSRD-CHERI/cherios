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
#ifndef CHERIOS_VIRTIO_NET_H_H
#define CHERIOS_VIRTIO_NET_H_H

#include "virtio_types.h"
#include "mips.h"
#include "cheric.h"
#include "virtio_queue.h"

#define VIRTIO_NET_F_CSUM (0)
// Device handles packets with partial checksum. This “checksum offload” is a common feature on modern network cards.

#define VIRTIO_NET_F_GUEST_CSUM (1)
// Driver handles packets with partial checksum.

#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS (2)
// Control channel offloads reconfiguration support.

#define VIRTIO_NET_F_MAC (5)
// Device has given MAC address.

#define VIRTIO_NET_F_GUEST_TSO4 (7)
// Driver can receive TSOv4.

#define VIRTIO_NET_F_GUEST_TSO6 (8)
// Driver can receive TSOv6.

#define VIRTIO_NET_F_GUEST_ECN (9)
// Driver can receive TSO with ECN.

#define VIRTIO_NET_F_GUEST_UFO (10)
// Driver can receive UFO.

#define VIRTIO_NET_F_HOST_TSO4 (11)
// Device can receive TSOv4.

#define VIRTIO_NET_F_HOST_TSO6 (12)
// Device can receive TSOv6.

#define VIRTIO_NET_F_HOST_ECN (13)
// Device can receive TSO with ECN.

#define VIRTIO_NET_F_HOST_UFO (14)
// Device can receive UFO.

#define VIRTIO_NET_F_MRG_RXBUF (15)
// Driver can merge receive buffers.

#define VIRTIO_NET_F_STATUS (16)
// Configuration status field is available.

#define VIRTIO_NET_F_CTRL_VQ (17)
// Control channel is available.

#define VIRTIO_NET_F_CTRL_RX (18)
// Control channel RX mode support.

#define VIRTIO_NET_F_CTRL_VLAN (19)
// Control channel VLAN filtering.

#define VIRTIO_NET_F_GUEST_ANNOUNCE (21)
// Driver can send gratuitous packets.

#define VIRTIO_NET_F_MQ (22)
// Device supports multiqueue with automatic receive steering.

#define VIRTIO_NET_F_CTRL_MAC_ADDR (23)
// Set MAC address through control channel.



#define VIRTIO_NET_S_LINK_UP        1
#define VIRTIO_NET_S_ANNOUNCE       2

struct virtio_net_config {
    u8 mac[6];
    le16 status;
    le16 max_virtqueue_pairs;
};

struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM     1
    u8 flags;
#define VIRTIO_NET_HDR_GSO_NONE         0
#define VIRTIO_NET_HDR_GSO_TCPV4        1
#define VIRTIO_NET_HDR_GSO_UDP          3
#define VIRTIO_NET_HDR_GSO_TCPV6        4
#define VIRTIO_NET_HDR_GSO_ECN          0x80
    u8 gso_type;
    le16 hdr_len;
    le16 gso_size;
    le16 csum_start;
    le16 csum_offset;
    le16 num_buffers;
};
/*
struct virtio_net_ctrl {
    u8 class;
    u8 command;
    u8 command_specific_data[];
    u8 ack;
};*/

/* ack values */
#define VIRTIO_NET_OK     0
#define VIRTIO_NET_ERR    1

#define VIRTIO_NET_CTRL_RX              0
#define VIRTIO_NET_CTRL_RX_PROMISC      0
#define VIRTIO_NET_CTRL_RX_ALLMULTI     1
#define VIRTIO_NET_CTRL_RX_ALLUNI       2
#define VIRTIO_NET_CTRL_RX_NOMULTI      3
#define VIRTIO_NET_CTRL_RX_NOUNI        4
#define VIRTIO_NET_CTRL_RX_NOBCAST      5

/*
struct virtio_net_ctrl_mac {
    le32 entries;
    u8 macs[entries][6];
};*/

#define VIRTIO_NET_CTRL_MAC             1
#define VIRTIO_NET_CTRL_MAC_TABLE_SET   0
#define VIRTIO_NET_CTRL_MAC_ADDR_SET    1

#define VIRTIO_NET_CTRL_VLAN            2
#define VIRTIO_NET_CTRL_VLAN_ADD        0
#define VIRTIO_NET_CTRL_VLAN_DEL        1

#define VIRTIO_NET_CTRL_ANNOUNCE        3
#define VIRTIO_NET_CTRL_ANNOUNCE_ACK    0

struct virtio_net_ctrl_mq {
    le16 virtqueue_pairs;
};
#define VIRTIO_NET_CTRL_MQ              4
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET 0
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN 1
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX 0x8000

le64 offloads;
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS          5
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET      0

void virtio_daemon_start(void);

#endif //CHERIOS_VIRTIO_NET_H_H
