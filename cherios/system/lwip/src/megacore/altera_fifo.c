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
#include "lightweight_ccall.h"
#include "lwip/inet_chksum.h"

int altera_transport_init(net_session* session) {
    session->irq = MEGA_CORE_IRQ_RECV_0;
    syscall_interrupt_register(session->irq, act_self_ctrl, -1, 0, session);
    return 0;
}

int ienabled = 0;

void lwip_driver_enable_interrupts(__unused net_session* session) {

    // Ack the interrupt
    if(!ienabled) session->mmio->recv_fifo.ctrl_ie = NTOH32(A_ONCHIP_FIFO_MEM_CORE_INTR_ALMOSTFULL);
    ienabled = 1;
}

void lwip_driver_disable_interrupts(__unused net_session* session) {

    // Ack the interrupt
    if(ienabled) session->mmio->recv_fifo.ctrl_ie =  0;
    ienabled = 0;
}


int lwip_driver_init_postup(net_session* session) {
    // TODO once we work out hor interrupts work - this is where we should enable
    // Enable interrupts on the receive to enable interrupts when fifo not empty
    ALTERA_FIFO* rx_fifo = &session->mmio->recv_fifo;

    rx_fifo->ctrl_almostfull = NTOH32(1); // We want an interrupt when anything arrives
    lwip_driver_enable_interrupts(session);

    syscall_interrupt_enable(session->irq, act_self_ctrl);

    return 0;
}

#define NTOH32_SE(X) ({uint32_t _tmp = X; _tmp = NTOH32(_tmp); _tmp;})

void lwip_driver_handle_interrupt(net_session* session, __unused register_t arg, __unused register_t irq) {

    // Disable further interrupts

    lwip_driver_disable_interrupts(session);

    // Ack the interrupt

    ALTERA_FIFO* rx_fifo = &session->mmio->recv_fifo;

    rx_fifo->ctrl_i_event = NTOH32(A_ONCHIP_FIFO_MEM_CORE_INTR_ALMOSTFULL);

    // Renable interrupts with the OS
    if(irq != (register_t)-1) syscall_interrupt_enable((int)irq, act_self_ctrl);

    // To read: Read data (triggers change of meta), then you can read meta

    custom_for_tcp* custom = NULL;
    uint32_t* payload;

    u16_t len = 0;

    MAC_DWORD min_fill = NTOH32_SE(rx_fifo->ctrl_fill_level);

    while(min_fill != 0) {

        uint32_t data = rx_fifo->symbols;
        uint32_t meta = rx_fifo->metadata;

        min_fill--;

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

        if(min_fill == 0) min_fill = NTOH32_SE(rx_fifo->ctrl_fill_level);
    }

    if(custom != NULL) {
        printf(KRED"FREE BROKEN PACKET\n");
        pbuf_free(&custom->as_pbuf.custom.pbuf);
    }

    return;
}

// Calls a handwritten output. Don't need
err_t lwip_driver_output_secure(struct netif *netif, struct pbuf *p) {
    net_session* session = netif->state;

    ALTERA_FIFO* tx_fifo = &session->mmio->tran_fifo;

    return LIGHTWEIGHT_CCALL_FUNC(r, driver_output_func, CHK_DATA, 0, 2, tx_fifo, p);
}

// Uses the unsealing type it should not have access to
err_t lwip_driver_output(struct netif *netif, struct pbuf *p) {

    // To write: Set meta (if needed), then set symbols
    net_session* session = netif->state;

    ALTERA_FIFO* tx_fifo = &session->mmio->tran_fifo;

    tx_fifo->metadata = NTOH32(A_ONCHIP_FIFO_MEM_CORE_SOP);

    int more;

    uint64_t read_buf = 0; // holds partial words
    uint32_t got_bits = 0;

    MAC_DWORD max_depth = NTOH32_SE(tx_fifo->ctrl_fill_level);

    do {
        // Pbufs have an extra sealed payload. If sp_length is non zero it is a part of the payload, interted at some offset
        // Otherwise it authorises the entire payload, and the payload will be untagged
        uint8_t* buf = p->payload;
        uint8_t* sbuf = p->sealed_payload;
        uint16_t bufLen = p->len;

        uint16_t lenA = bufLen;
        uint16_t lenB, lenC;

        uint8_t* payloadA = buf;
        uint8_t* payloadB = NULL;
        uint8_t* payloadC = NULL;

        more = p->len != p->tot_len;

        if(sbuf) {
            sbuf = cheri_unseal(sbuf, ether_sealer);
            if(p->sp_length) {
                lenA = p->sp_dst_offset;
                lenB = p->sp_length;
                lenC = bufLen - (lenA + lenB);
                payloadB = sbuf + p->sp_src_offset;
                payloadC = buf + (lenA + lenB);
            } else {
                payloadA = sbuf + (size_t)(buf - sbuf);
            }
        }

        do {
            uint8_t *payload_8 = payloadA;
            uint16_t len = lenA;
            size_t align_off = (-cheri_getcursor(payload_8)) & 0x3;

            uint8_t frag_more = more || (payloadB != NULL);
            // Align source to 4 bytes

            if (align_off & 1) {
                read_buf = (read_buf << 8) | (*payload_8);
                got_bits += 8;
                payload_8 += 1;
                len -= 1;
            }
            if (len >= 2 && align_off & 2) {
                read_buf = (read_buf << 16) | (*((uint16_t *) payload_8));
                got_bits += 16;
                payload_8 += 2;
                len -= 2;
            }

#define WRITE                                                                           \
        word = (uint32_t)((read_buf >> (got_bits)) & 0xFFFFFFFF);                       \
        while(max_depth == AVALON_FIFO_TX_BASIC_OPTS_DEPTH) {max_depth = NTOH32_SE(tx_fifo->ctrl_fill_level);} \
        if(len == 0 && (got_bits == 0) && !frag_more) tx_fifo->metadata = NTOH32(A_ONCHIP_FIFO_MEM_CORE_EOP);   \
        tx_fifo->symbols = word;
        max_depth++;

            uint32_t word;

            if (got_bits >= 32) {
                got_bits -= 32;
                WRITE
            }

            // Do fast 4 byte chunks
            uint32_t *payload_32 = (uint32_t *) payload_8;

            while (len >= 4) {
                read_buf = (read_buf << 32) | (uint64_t) *(payload_32++);
                len -= 4;
                WRITE
            }

            // Handle (up to) last 3 bytes
            payload_8 = (uint8_t *) payload_32;

            if (len & 2) {
                read_buf = (read_buf << 16) | (*((uint16_t *) payload_8));
                got_bits += 16;
                payload_8 += 2;
                len -= 2;
            }

            if (len & 1) {
                read_buf = (read_buf << 8) | (*payload_8);
                got_bits += 8;
                len -= 1;
            }

            if (got_bits >= 32) {
                got_bits -= 32;
                WRITE
            }

            assert_int_ex(len, ==, 0);

            payloadA = payloadB;
            payloadB = payloadC;
            payloadC = NULL;
            lenA = lenB;
            lenB = lenC;
        } while(payloadA);

    } while(more && (p = p->next));

    if(got_bits) {
        uint32_t missing = 32 - got_bits;
        got_bits /= 8;
        if(max_depth == AVALON_FIFO_TX_BASIC_OPTS_DEPTH)
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
        // TODO handle sealed payload
        uint32_t* payload = (uint32_t*)p->payload;
        uint32_t* end = (uint32_t*)((char*)p->payload + p->len);

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