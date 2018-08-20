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

int lwip_driver_init(net_session* session) {
    // TODO
    mac_control* ctrl = session->mmio;

    // Use scratch register to check we have mapped the right place
    MAC_DWORD test = 0xfefe;
    ctrl->base_config.scratch = test;
    if(test != ctrl->base_config.scratch) return -1;


    // Do an initial reset to clear everything
    ctrl->base_config.command_config = CC_N_MASK(COMMAND_CONFIG_SW_RESET) | CC_N_MASK(COMMAND_CONFIG_CNT_RESET);

    // Wait for it to happen (should only take a few cycles so we don't sleep)
    while(ctrl->base_config.command_config & CC_N_MASK(COMMAND_CONFIG_SW_RESET));

    MAC_DWORD mac0 = (((((session->mac[0] << 8) | session->mac[1]) << 8) | session->mac[2]) << 8) | session->mac[3];
    MAC_DWORD mac1 = ((session->mac[4] << 8) | session->mac[5]) << 16;

    ctrl->base_config.mac_0 = mac0;
    ctrl->base_config.mac_1 = mac1;

    for(size_t i = 0; i < 4; i++) {
        ctrl->smacs[(2*i) + 0] = mac0;
        ctrl->smacs[(2*i) + 1] = mac1;
    }

    MAC_DWORD config = 0;

    // Also for 32bit align

    config = CC_N_MASK(COMMAND_CONFIG_PAD_EN);

    ctrl->base_config.command_config = config;

    // These will shift alignment to achieve 32 bit align for ethernet headers

    MAC_DWORD tx_cmd = CC_N_MASK(CMD_STAT_TX_SHIFT16);
    MAC_DWORD rx_cmd = CC_N_MASK(CMD_STAT_RX_SHIFT16);

    ctrl->tx_cmd_stat = tx_cmd;
    ctrl->rx_cmd_stat = rx_cmd;

    // Finally enable TX/RX

    config |= CC_N_MASK(COMMAND_CONFIG_TX_ENA) | CC_N_MASK(COMMAND_CONFIG_RX_ENA);

    ctrl->base_config.command_config = config;

    MAC_DWORD got_config = ctrl->base_config.command_config;
    MAC_DWORD tx_cmd_got = ctrl->tx_cmd_stat;
    MAC_DWORD rx_cmd_got = ctrl->rx_cmd_stat;

    // Check we got the config options we asked for
    if(config != got_config || tx_cmd_got != tx_cmd || rx_cmd_got != rx_cmd) return -2;

    return 0;
}

int lwip_driver_init_postup(net_session* session) {
    // TODO once we work out hor interrupts work - this is where we should enable
    // Enable interrupts on the receive to enable interrupts when fifo not empty
    ALTERA_FIFO* rx_fifo = &session->mmio->recv_fifo;

    rx_fifo->ctrl_almostfull = NTOH32(1); // We want an interrupt when anything arrives
    rx_fifo->ctrl_ie = NTOH32(A_ONCHIP_FIFO_MEM_CORE_INTR_ALMOSTFULL);
    return 0;
}

void lwip_driver_handle_interrupt(net_session* session) {
    // To read: Read data (triggers change of meta), then you can read meta

    ALTERA_FIFO* rx_fifo = &session->mmio->recv_fifo;

    custom_for_tcp* custom = NULL;
    uint32_t* payload;

    u16_t len = 0;

    while(rx_fifo->ctrl_fill_level != 0) {



        uint32_t data = rx_fifo->symbols;
        uint32_t meta = rx_fifo->metadata;

        uint32_t sop = meta & (NTOH32(A_ONCHIP_FIFO_MEM_CORE_SOP));
        uint32_t eop = meta & (NTOH32(A_ONCHIP_FIFO_MEM_CORE_EOP));

        if(sop) {
            if(custom != NULL) {
                printf(KRED"FREE BROKEN PACKET\n"KRED);
                pbuf_free(&custom->as_pbuf.custom.pbuf);
            }
            custom = alloc_custom(session);
            payload = (uint32_t*)custom->as_pbuf.custom.pbuf.payload;
            len = 0;
        }

        if(custom) {
            *(payload++) = data;
            len +=4;
        }

        if(eop || len == CUSTOM_BUF_PAYLOAD_SIZE) {
            uint16_t empty = (uint16_t)(NTOH32(meta) & A_ONCHIP_FIFO_MEM_CORE_EMPTY_MASK) >> A_ONCHIP_FIFO_MEM_CORE_EMPTY_SHIFT;
            len -=empty;

            if(custom) {
                if(eop) {
                    // Hand up to LWIP
                    custom->as_pbuf.custom.pbuf.len = len;
                    session->nif->input(&custom->as_pbuf.custom.pbuf, session->nif);
                } else {
                    printf(KRED"FREE BROKEN PACKET (too long)\n"KRST);
                    pbuf_free(&custom->as_pbuf.custom.pbuf);
                }

            }
            custom = NULL;
        }
    }

    if(custom != NULL) {
        printf(KRED"FREE BROKEN PACKET\n");
        pbuf_free(&custom->as_pbuf.custom.pbuf);
    }

    // Ack the interrupt

    rx_fifo->ctrl_i_event = NTOH32(A_ONCHIP_FIFO_MEM_CORE_INTR_ALMOSTFULL);

    return;
}

err_t lwip_driver_output(struct netif *netif, struct pbuf *p) {

    // To write: Set meta (if needed), then set symbols
    net_session* session = netif->state;

    ALTERA_FIFO* tx_fifo = &session->mmio->tran_fifo;

    tx_fifo->metadata = NTOH32(A_ONCHIP_FIFO_MEM_CORE_SOP);

    int more;

    uint64_t read_buf = 0; // holds partial words
    uint32_t got_bits = 0;

    do {
        uint8_t * payload_8 = (uint8_t*)p->payload;

        size_t align_off = (-cheri_getcursor(payload_8)) & 0x3;

        more = p->len != p->tot_len;
        uint16_t len = p->len;

        // Align source to 4 bytes

        if(align_off & 1) {
            read_buf = (read_buf << 8) | (*payload_8);
            got_bits+=8;
            payload_8+=1;
            len-=1;
        }
        if(len >= 2 && align_off & 2) {
            read_buf = (read_buf << 16) | (*((uint16_t*)payload_8));
            got_bits+=16;
            payload_8+=2;
            len-=2;
        }

#define WRITE                                                                           \
        word = (uint32_t)((read_buf >> (got_bits)) & 0xFFFFFFFF);                       \
        while(tx_fifo->ctrl_fill_level == AVALON_FIFO_TX_BASIC_OPTS_DEPTH);             \
        if(len == 0 && !more) tx_fifo->metadata = NTOH32(A_ONCHIP_FIFO_MEM_CORE_EOP);   \
        tx_fifo->symbols = word;

        uint32_t word;

        if(got_bits >= 32) {
            got_bits-=32;
            WRITE
        }

        // Do fast 4 byte chunks
        uint32_t* payload_32 = (uint32_t*)payload_8;

        while(len >= 4) {
            read_buf = (read_buf << 32) | (uint64_t)*(payload_32++);
            len -=4;
            WRITE
        }

        // Handle (up to) last 3 bytes
        payload_8 = (uint8_t*)payload_32;

        if(len & 2) {
            read_buf = (read_buf << 16) | (*((uint16_t*)payload_8));
            got_bits+=16;
            payload_8+=2;
            len-=2;
        }

        if(len & 1) {
            read_buf = (read_buf << 8) | (*payload_8);
            got_bits+=8;
            len-=1;
        }

        if(got_bits >= 32) {
            got_bits-=32;
            WRITE
        }

        assert_int_ex(len, ==, 0);

    } while(more && (p = p->next));

    if(got_bits) {
        uint32_t missing = 32 - got_bits;
        got_bits /= 8;
        while(tx_fifo->ctrl_fill_level == AVALON_FIFO_TX_BASIC_OPTS_DEPTH);
        tx_fifo->metadata = NTOH32(A_ONCHIP_FIFO_MEM_CORE_EOP |
                                           ((4 - (got_bits)) << A_ONCHIP_FIFO_MEM_CORE_EMPTY_SHIFT));
        tx_fifo->symbols = (uint32_t)((read_buf << missing) & 0xFFFFFFFF);
    }

    return ERR_OK;
}

err_t lwip_driver_output_aligned(struct netif *netif, struct pbuf *p) {

    // To write: Set meta (if needed), then set symbols
    net_session* session = netif->state;

    ALTERA_FIFO* tx_fifo = &session->mmio->tran_fifo;

    tx_fifo->metadata = NTOH32(A_ONCHIP_FIFO_MEM_CORE_SOP);

    int more;

    do {
        uint32_t* payload = (uint32_t*)p->payload;
        uint32_t* end = (uint32_t*)(p->payload + p->len);

        more = p->len != p->tot_len;

        while(payload != end) {
            uint32_t word = *payload;
            payload++;
            while(tx_fifo->ctrl_fill_level == AVALON_FIFO_TX_BASIC_OPTS_DEPTH);
            if(payload == end && !more) tx_fifo->metadata = NTOH32(A_ONCHIP_FIFO_MEM_CORE_EOP);
            tx_fifo->symbols = word;
        }


    } while(more && (p = p->next));

    return ERR_OK;
}

int lwip_driver_poll(net_session* session) {
    ALTERA_FIFO* rx_fifo = &session->mmio->recv_fifo;

    return (rx_fifo->ctrl_fill_level != 0);
}