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

#include <mega_core.h>
#include "lwip_driver.h"
#include "mman.h"
#include "endian.h"


//FIXME don't use hit WB it doesn't work on cheri. use index WB
static void wb_cache(size_t addr, size_t length) {
    size_t end = addr + length;
    size_t base = (addr) & ~(L2_LINE_SIZE-1);

    do {
        CACHE_OP(CACHE_OP_ADDR_HIT_WB(CACHE_L1_DATA), 0, base);
        CACHE_OP(CACHE_OP_ADDR_HIT_WB(CACHE_L2), 0, base);
        base += L1_LINE_SIZE;
    } while(base < end);
}

static void wb_inv_cache(size_t addr, size_t length) {
    size_t end = addr + length;
    size_t base = (addr) & ~(L2_LINE_SIZE-1);

    do {
        CACHE_OP(CACHE_OP_ADDR_HIT_WB_INVAL(CACHE_L1_DATA), 0, base);
        CACHE_OP(CACHE_OP_ADDR_HIT_WB_INVAL(CACHE_L2), 0, base);
        base += L1_LINE_SIZE;
    } while(base < end);
}

// This one is unsafe and will probably be removed. Use wb_inv instead which is much safer
static void inv_cache(size_t addr, size_t length) {

    size_t end = addr + length;
    size_t base = (addr) & ~(L2_LINE_SIZE-1);

    do {
        CACHE_OP(CACHE_OP_ADDR_HIT_INVAL(CACHE_L1_DATA), 0, base);
        CACHE_OP(CACHE_OP_ADDR_HIT_INVAL(CACHE_L2), 0, base);
        base += L1_LINE_SIZE;
    } while(base < end);


}

static void alloc_rx_buf(net_session* session, msgdma_desc* desc, struct pubf** buf_map, uint32_t control) {
    struct custom_for_tcp* pbf_new = alloc_custom(session);
    size_t pbuf_new_addr = ((size_t)pbf_new->as_pbuf.custom.pbuf.payload) + pbf_new->offset;
    desc->write_lo = HTOLE32(pbuf_new_addr);
    *buf_map = &pbf_new->as_pbuf.custom.pbuf;

    control |= HTOLE32(CONTROL_OWN | CONTROL_GO);

    // Its very important that we do not touch any of the payload while DMA is taking place
    // We have hopefully contained the payload entirely in its own set of cache lines
    inv_cache((size_t)pbf_new->as_pbuf.custom.pbuf.payload,CUSTOM_BUF_PAYLOAD_SIZE);

    desc->control = control;
}

static void setup_desc_ring(msgdma_desc* ring, uint64_t phyoff, size_t length, uint32_t init_length, uint32_t init_control) {
    for(size_t i = 0; i != length; i++) {
        size_t next_i = (i+1) & (length-1);
        size_t next_paddr = ((size_t)&ring[next_i]) + phyoff;
        ring[i].length = init_length;
        ring[i].next = HTOLE32(next_paddr);
        ring[i].control = init_control;
    }
}

static void setup_descs(net_session* session) {
    // Create a ring buffer for RX
    // TODO map the descriptors as uncached

    size_t need_bytes = sizeof(msgdma_desc) * (SGDMA_DESCS_RX + SGDMA_DESCS_TX);
    size_t phy_off;
    cap_pair pair;

    ERROR_T(res_t) res = mem_request_phy_out(0, need_bytes, COMMIT_DMA | COMMIT_UNCACHED, own_mop, &phy_off);

    assert(IS_VALID(res));

    rescap_take(res.val, &pair);

    phy_off -= ((size_t)(pair.data) - RES_META_SIZE);

    session->rx_descs = (msgdma_desc*)cheri_setbounds(pair.data, sizeof(msgdma_desc) * SGDMA_DESCS_RX);
    session->tx_descs = (msgdma_desc*)cheri_setbounds(pair.data + (sizeof(msgdma_desc) * SGDMA_DESCS_RX), sizeof(msgdma_desc) * SGDMA_DESCS_TX);

    session->tx_freed_index = session->tx_free_index = session->rx_index = 0;

    // TODO align so these don't fail
    assert_int_ex(((size_t)session->rx_descs) & (sizeof(msgdma_desc)-1), ==, 0);
    assert_int_ex(((size_t)session->tx_descs) & (sizeof(msgdma_desc)-1), ==, 0);

    // Do I need go?
    uint32_t control = 0;

    setup_desc_ring(session->tx_descs, phy_off, SGDMA_DESCS_TX, 0, control);

    control = HTOLE32(CONTROL_ET_IRQ_EN | CONTROL_TC_IRQ_EN | CONTROL_END_ON_EOP);

    setup_desc_ring(session->rx_descs, phy_off, SGDMA_DESCS_RX, HTOLE32(CUSTOM_BUF_PAYLOAD_SIZE), control);

    // RX prefill. Leave the last un-used

    for(size_t i = 0; i != SGDMA_DESCS_RX-1; i++) {
        alloc_rx_buf(session, &session->rx_descs[i], &session->pbuf_rx_map[i], control);
    }

    session->rx_index = SGDMA_DESCS_RX;

    // TX/RX set head

    sgdma_mmio* sgdma = session->sgdma_mmio;

    size_t rx_addr = ((size_t)session->rx_descs) + phy_off;
    size_t tx_addr = ((size_t)session->tx_descs) + phy_off;

    sgdma->rx_scp.next_desc_ptr_lo = HTOLE32((uint32_t)rx_addr);
    sgdma->rx_scp.poll_freq = HTOLE32(POLL_FREQ);

    sgdma->tx_scp.next_desc_ptr_lo = HTOLE32((uint32_t)tx_addr);
    sgdma->tx_scp.poll_freq = HTOLE32(POLL_FREQ);

    sgdma->rx_scp.control = HTOLE32(PF_CONTROL_GIEM | PF_CONTROL_DESC_POLL_EN | PF_CONTROL_RUN);
    sgdma->tx_scp.control = HTOLE32(PF_CONTROL_GIEM | PF_CONTROL_DESC_POLL_EN | PF_CONTROL_RUN);
}

// Handles any RX packets
static void handle_rx(net_session* session) {
    uint32_t control;

    while((control = (&session->rx_descs[session->rx_index % SGDMA_DESCS_RX])->control), !(control & HTOLE32(CONTROL_OWN))) {

        // Create a new one straight away (we keep the previous exit un-used so one always is
        alloc_rx_buf(session, &session->rx_descs[(session->rx_index-1) % SGDMA_DESCS_RX], &session->pbuf_rx_map[(session->rx_index-1) % SGDMA_DESCS_RX], control);


        msgdma_desc* desc = &session->rx_descs[session->rx_index % SGDMA_DESCS_RX];
        struct pubf** buf_map = &session->pbuf_rx_map[session->rx_index % SGDMA_DESCS_RX];

        // Grab the pbuf that has been filled
        struct pbuf* pbuf_in = *buf_map;
        uint32_t transferred = desc->transferred;
        transferred = HTOLE32(transferred);

        pbuf_in->len = (uint16_t)transferred;

        // Hand up to lwip

        session->nif->input(pbuf_in, session->nif);
        session->rx_index++;
    }

    // Ack interrupt

    session->sgdma_mmio->rx_scp.status = HTOLE32(PF_STATUS_IRQ);
}

// Frees pbufs from sent tx packets
static void handle_tx(net_session* session) {
    uint32_t control;
    while((session->tx_freed_index != session->tx_free_index) &&
            ((control = (&session->tx_descs[session->tx_freed_index % SGDMA_DESCS_TX])->control),
                    !(control & HTOLE32(CONTROL_OWN)))) {
        // Get the pbuf that has been sent and free it.
        struct pbuf* pbuf = session->pbuf_tx_map[session->tx_freed_index % SGDMA_DESCS_TX];
        if(pbuf) {
            pbuf_free(pbuf);
        }
        //TODO error check
        session->tx_freed_index++;
    }
}

int altera_transport_init(net_session* session) {
    cap_pair pair;

    // Get sgdma_mmio
    get_physical_capability(MSGDMA_BASE, MSGDMA_SIZE, 1, 0, own_mop, &pair);

    session->sgdma_mmio = (sgdma_mmio*)pair.data;

    // Init the sgdma...

    setup_descs(session);

    // Enable interrupts
    session->irq = MSGDMA_RX_IRQ;

    syscall_interrupt_register(session->irq, act_self_ctrl, -1, 0, session);
    //syscall_interrupt_register(MSGDMA_TX_IRQ, act_self_ctrl, -1, 0, session);

    return 0;
}

int lwip_driver_init_postup(net_session* session) {

    syscall_interrupt_enable(session->irq, act_self_ctrl);
    //syscall_interrupt_enable(MSGDMA_TX_IRQ, act_self_ctrl);

    return 0;
}

void lwip_driver_handle_interrupt(net_session* session, register_t arg, register_t irq) {

    handle_tx(session);

    handle_rx(session);

    syscall_interrupt_enable((int)irq, act_self_ctrl);
}

int sgdma_handle_func(capability arg, phy_handle_flags flags, size_t phy_addr, size_t length) {
    net_session* session = (net_session*)arg;
    assert(session->tx_free_index - session->tx_freed_index < SGDMA_DESCS_TX - 1); // Just hope we have a descriptor for this...


    session->pbuf_tx_map[session->tx_free_index % SGDMA_DESCS_TX] = NULL;
    msgdma_desc* desc = &session->tx_descs[session->tx_free_index % SGDMA_DESCS_TX];

    // FIXME: Seems to always do the nearest 32-byte aligned transfer

    desc->read_lo = HTOLE32(phy_addr);
    desc->length = HTOLE32(length);

    uint32_t control = HTOLE32(CONTROL_GO | CONTROL_OWN);

    if(flags & PHY_HANDLE_SOP) control |= HTOLE32(CONTROL_GEN_SOP);
    if(flags & PHY_HANDLE_EOP) control |= HTOLE32(CONTROL_GEN_EOP);

    desc->control = control;

    session->tx_free_index++;

    return 0;
}

err_t lwip_driver_output(struct netif *netif, struct pbuf *p) {
    // Write descriptors and setup callback
    net_session* session = netif->state;

    handle_tx(session); // free as many txs as possible
    int more;

    struct pbuf* for_map = p;

    phy_handle_flags flags = PHY_HANDLE_SOP;

    size_t ndx = session->tx_free_index % (SGDMA_DESCS_TX);

    do {
        more = p->len != p->tot_len;

        char* payload = p->payload;

        payload[-1]++;
        payload[0] = 0xb;
        payload[1] = 0xc;

        uint16_t length = p->len;

        // Force writeback before any DMA
        // wb_cache((size_t)payload, length);

        if(!more) flags |= PHY_HANDLE_EOP;

        // Setup descriptors over each contiguous physical range

        int ret = for_each_phy((capability)session, flags, &sgdma_handle_func, payload+16, length);

        assert_int_ex(ret, >=, 0);

        flags = PHY_HANDLE_NONE;
    } while(more && (p = p->next));

    // Ref the head
    pbuf_ref(for_map);
    session->pbuf_tx_map[ndx] = for_map;

    return ERR_OK;
}

int lwip_driver_poll(net_session* session) {
    return ((&session->rx_descs[session->rx_index % SGDMA_DESCS_RX])->control & HTOLE32(CONTROL_OWN)) == 0;
}