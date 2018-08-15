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
#ifndef CHERIOS_LWIP_DRIVER_H
#define CHERIOS_LWIP_DRIVER_H

#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/inet.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/init.h"
#include "lwip/stats.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip4_frag.h"
#include "lwip/apps/httpd.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#include "cp0.h"

#ifdef HARDWARE_qemu

#include "virtio.h"
#include "virtio_net.h"

#else

#include "a_api.h"
#include "mega_core.h"

#endif

// This driver IF is a stepping stone to get lwip working. It will eventually be extracted into a stand-alone driver
// and they will communicate via the socket API.

#define CHERIOS_NET_MASK    "255.255.255.0"
#define CHERIOS_GATEWAY     "128.232.18.1"

#ifdef HARDWARE_qemu

    #define QUEUE_SIZE 0x10
    #define CHERIOS_IP          "128.232.18.56"
    #define CHERIOS_MAC         {0x00,0x16,0x3E,0xE8,0x12,0x38}
    #define CHERIOS_HOST        "cherios"

    typedef virtio_mmio_map lwip_driver_mmio_t;
#else

    #define CHERIOS_IP          "128.232.18.245"
    #define CHERIOS_MAC         {0xba,0xdb,0xab,0xe5,0xca,0xfe}
    #define CHERIOS_HOST        "cherios-fpga"

    typedef void lwip_driver_mmio_t;

#endif

typedef struct net_session {

    struct netif* nif;
    lwip_driver_mmio_t* mmio;
    struct arena_t* dma_arena;
    ip4_addr_t gw_addr, my_ip, netmask;
    uint8_t mac[6];
    uint8_t irq;

#ifdef HARDWARE_qemu

    // Net headers for send and recieve (SEND_HDR_START for recv, second lot for send)
#define NET_HDR_P(N) session->net_hdrs_paddr + (sizeof(struct virtio_net_hdr) * (N))
#define SEND_HDR_START (QUEUE_SIZE)
    struct virtio_net_hdr* net_hdrs;
    struct virtq virtq_send, virtq_recv;
    size_t net_hdrs_paddr;
    struct virtio_net_config config;
    le16 free_head_send;
    le16 free_head_recv;
    le16 recvs_free;
    struct pbuf* pbuf_recv_map[QUEUE_SIZE];
    struct pbuf* pbuf_send_map[QUEUE_SIZE];
#else

#endif

} net_session;

#define DEFAULT_USES 64;

typedef struct custom_for_tcp {
    union {
        struct {
            struct pbuf_custom custom;
            char buf[TCP_MSS + PBUF_TRANSPORT];
        } as_pbuf;
        struct {
            struct custom_for_tcp* next_free;
        } as_free;
    };
    uint64_t reuse;
    size_t offset;
} custom_for_tcp;


// Generic things

struct custom_for_tcp* alloc_custom(net_session* session);

// Per driver

int lwip_driver_init(net_session* session);
int lwip_driver_init_postup(net_session* session);
void lwip_driver_handle_interrupt(net_session* session);
err_t lwip_driver_output(struct netif *netif, struct pbuf *p);
int lwip_driver_poll(net_session* session);

#endif //CHERIOS_LWIP_DRIVER_H
