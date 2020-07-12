/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2018 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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

#ifndef	_VIRTIO_MMIO_H
#define	_VIRTIO_MMIO_H

#include "cheric.h"

#define VIRTIO_MMIO_SIZE            0x200

#define	VIRTIO_MMIO_MAGIC_VALUE		0x000
#define	VIRTIO_MMIO_VERSION		0x004
#define	VIRTIO_MMIO_DEVICE_ID		0x008
#define	VIRTIO_MMIO_VENDOR_ID		0x00c
#define	VIRTIO_MMIO_HOST_FEATURES	0x010
#define	VIRTIO_MMIO_HOST_FEATURES_SEL	0x014
#define	VIRTIO_MMIO_GUEST_FEATURES	0x020
#define	VIRTIO_MMIO_GUEST_FEATURES_SEL	0x024
#define	VIRTIO_MMIO_GUEST_PAGE_SIZE	0x028
#define	VIRTIO_MMIO_QUEUE_SEL		0x030
#define	VIRTIO_MMIO_QUEUE_NUM_MAX	0x034
#define	VIRTIO_MMIO_QUEUE_NUM		0x038
#define	VIRTIO_MMIO_QUEUE_ALIGN		0x03c
#define	VIRTIO_MMIO_QUEUE_PFN		0x040
#define	VIRTIO_MMIO_QUEUE_READY		0x044
#define	VIRTIO_MMIO_QUEUE_NOTIFY	0x050
#define	VIRTIO_MMIO_INTERRUPT_STATUS	0x060
#define	VIRTIO_MMIO_INTERRUPT_ACK	0x064
#define	VIRTIO_MMIO_STATUS		0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW	0x80
#define VIRTIO_MMIO_QUEUE_DESC_HIGH	0x84
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW	0x90
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH	0x94
#define VIRTIO_MMIO_QUEUE_USED_LOW	0xA0
#define VIRTIO_MMIO_QUEUE_USED_HIGH	0xA4
#define	VIRTIO_MMIO_CONFIG		0x100
#define	VIRTIO_MMIO_INT_VRING		(1 << 0)
#define	VIRTIO_MMIO_INT_CONFIG		(1 << 1)
#define	VIRTIO_MMIO_VRING_ALIGN		4096

#define STATUS_ACKNOWLEDGE		(1)
#define STATUS_DRIVER			(2)
#define STATUS_DRIVER_OK		(4)
#define STATUS_FEATURES_OK      (8)
#define STATUS_DEVICE_NEEDS_RESET	(64)

#define VIRTIO_MAGIC         0x74726976

typedef struct virtio_mmio_map {
    volatile u32 magic_value;
    volatile u32 version;
    volatile u32 device_id;
    volatile u32 vendor_id;
    volatile u32 host_features;
    volatile u32 host_features_sel;
    volatile u32 pad0, pad1;
    volatile u32 guest_features;
    volatile u32 guest_features_sel;
    volatile u32 guest_page_size;
    volatile u32 pad2;
    volatile u32 queue_sel;
    volatile u32 queue_num_max;
    volatile u32 queue_num;
    volatile u32 queue_align;
    volatile u32 queue_pfn;
    volatile u32 queue_ready;
    volatile u32 pad3, pad4;
    volatile u32 queue_notify;
    volatile u32 pad5, pad6, pad7;
    volatile u32 interrupt_status;
    volatile u32 interrupt_ack;
    volatile u32 pad8, pad9;
    volatile u32 status;
    volatile u32 pad10,pad11,pad12;
    volatile u32 queue_desc_low;
    volatile u32 queue_desc_high;
    volatile u32 pad13, pad14;
    volatile u32 queue_avail_low;
    volatile u32 queue_avail_high;
    volatile u32 pad15, pad16;
    volatile u32 queue_used_low;
    volatile u32 queue_used_high;
    volatile u32 pad17, pad18;
    volatile u32 pads[4*5];
    volatile char config[0x100];
} virtio_mmio_map;

_Static_assert(sizeof(virtio_mmio_map) == VIRTIO_MMIO_SIZE, "Get the struct size correct");

#endif /* _VIRTIO_MMIO_H */

