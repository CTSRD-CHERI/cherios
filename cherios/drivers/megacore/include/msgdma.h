/*-
 * Copyright (c) 2017-2018 Ruslan Bukin <br@bsdpad.com>
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
 *
 * $FreeBSD$
 */

/* Altera mSGDMA registers. */

#define CSR_STATUS_BUSY             (1 << 0)
#define CSR_STATUS_DESC_BUF_EMPTY   (1 << 1)
#define CSR_STATUS_DESC_BUG_FULL    (1 << 2)
#define CSR_STATUS_RESP_BUF_EMPTY   (1 << 3)
#define CSR_STATUS_RESP_BUF_FULL    (1 << 4)
#define CSR_STATUS_STOPPED          (1 << 5)
#define	CSR_STATUS_RESETTING	    (1 << 6)
#define CSR_STATUS_STOP_ER          (1 << 7)
#define CSR_STATUS_STOP_EARLY       (1 << 8)
#define CSR_STATUS_IRQ              (1 << 9)

#define CSR_CONTROL_STOP            (1 << 0)
#define	CSR_CONTROL_RESET		    (1 << 1) /* Reset Dispatcher */
#define CSR_CONTROL_STOP            (1 << 2)
#define CSR_CONTROL_STOP_ON_EARLY   (1 << 3)
#define	CSR_CONTROL_GIEM		    (1 << 4) /* Global Interrupt Enable Mask */
#define CSR_STOP_DESCRIPTORS        (1 << 5)


/* Descriptor fields. */
#define	CONTROL_GO		(1 << 31)	/* Commit all the descriptor info */
#define	CONTROL_OWN		(1 << 30)	/* Owned by hardware (prefetcher-enabled only) */
#define	CONTROL_EDE		(1 << 24)	/* Early done enable */
#define	CONTROL_ERR_S		16		/* Transmit Error, Error IRQ Enable */
#define	CONTROL_ERR_M		(0xff << CONTROL_ERR_S)
#define	CONTROL_ET_IRQ_EN	(1 << 15)	/* Early Termination IRQ Enable */
#define	CONTROL_TC_IRQ_EN	(1 << 14)	/* Transfer Complete IRQ Enable */
#define	CONTROL_END_ON_EOP	(1 << 12)	/* End on EOP */
#define	CONTROL_PARK_WR		(1 << 11)	/* Park Writes */
#define	CONTROL_PARK_RD		(1 << 10)	/* Park Reads */
#define	CONTROL_GEN_EOP		(1 << 9)	/* Generate EOP */
#define	CONTROL_GEN_SOP		(1 << 8)	/* Generate SOP */
#define	CONTROL_TX_CHANNEL_S	0		/* Transmit Channel */
#define	CONTROL_TX_CHANNEL_M	(0xff << CONTROL_TRANSMIT_CH_S)

/* Prefetcher */
#define	PF_CONTROL			0x00
#define	 PF_CONTROL_GIEM		(1 << 3)
#define	 PF_CONTROL_RESET		(1 << 2)
#define	 PF_CONTROL_DESC_POLL_EN	(1 << 1)
#define	 PF_CONTROL_RUN			(1 << 0)
#define	PF_NEXT_LO			0x04
#define	PF_NEXT_HI			0x08
#define	PF_POLL_FREQ			0x0C
#define	PF_STATUS			0x10
#define	 PF_STATUS_IRQ			(1 << 0)

/* Prefetcher-disabled descriptor format. */
typedef struct msgdma_desc_nonpf {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint32_t length;
    uint32_t control;
} msgdma_desc_nonpf;

/* Prefetcher-enabled descriptor format. */
typedef struct msgdma_desc {
    uint32_t read_lo;
    uint32_t write_lo;
    uint32_t length;
    uint32_t next;
    volatile uint32_t transferred;
    volatile uint32_t status;
    uint32_t reserved;
    volatile uint32_t control;
} msgdma_desc;

typedef struct {
    volatile uint32_t actual_bytes_transferred;
    volatile uint8_t error;
    volatile uint8_t early_termination;
} sgdma_response;


typedef struct {
    volatile uint32_t status;
    volatile uint32_t control;
    volatile uint16_t read_fill_level;
    volatile uint16_t write_fill_level;
    volatile uint16_t response_fill_level;
    volatile uint16_t reserved;
    volatile uint16_t read_req_no;
    volatile uint16_t write_seq_no;
    uint32_t pad[3];
} sgdma_csr;

typedef struct {
    volatile uint32_t control;
    volatile uint32_t next_desc_ptr_lo;
    volatile uint32_t next_desc_ptr_hi;
    volatile uint32_t poll_freq;
    volatile uint32_t status;
    uint32_t pad[3];
} sgdma_desc_poll;

// BERI register map

#define MSGDMA_BASE                 0x80004000
#define MSGDMA_SIZE                 0x00000a00

#define MSGDMA_RX_IRQ               13
#define MSGDMA_TX_IRQ               14

typedef struct sgdma_mmio {
    sgdma_csr rx_csr;
    sgdma_desc_poll rx_scp;
    char pad[0x40];
    sgdma_csr tx_csr;
    sgdma_desc_poll tx_scp;
} sgdma_mmio;

#define POLL_FREQ 1000