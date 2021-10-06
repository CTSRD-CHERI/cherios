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
#ifndef CHERIOS_VIRTIO_H
#define CHERIOS_VIRTIO_H

#include "virtio_mmio.h"
#include "virtio_queue.h"

#define DRIVER_BAD_DEVICE 					(-2)
#define DRIVER_BAD_FEATURES 				(-3)
#define DRIVER_QUEUE_TOO_LONG 				(-4)
#define DRIVER_QUEUE_MISSING_FIELDS         (-7)
#define DRIVER_QUEUE_CROSSES_PAGE_BOUNDRY 	(-5)
#define DRIVER_DEVICE_NEEDS_RESET			(-6)

// Oh yes, these are different on QEMU... (and device specific config is yet a third)
#define VIRTIO_IS_LITTLE_ENDIAN 1
#define VIRTIO_QUEUE_IS_LITTLE_ENDIAN 0

#if (VIRTIO_IS_LITTLE_ENDIAN)

#define VIRTIO_SWAP_U64(X)  __builtin_bswap64(X)
#define VIRTIO_SWAP_U32(X)  __builtin_bswap32(X)
#define VIRTIO_SWAP_U16(X)  __builtin_bswap16(X)

#else

#define VIRTIO_SWAP_U64(X) X
#define VIRTIO_SWAP_U32(X) X
#define VIRTIO_SWAP_U16(X) X

#endif

#if (VIRTIO_QUEUE_IS_LITTLE_ENDIAN)

#define VIRTIOQ_SWAP_U64(X)  __builtin_bswap64(X)
#define VIRTIOQ_SWAP_U32(X)  __builtin_bswap32(X)
#define VIRTIOQ_SWAP_U16(X)  __builtin_bswap16(X)

#else

#define VIRTIOQ_SWAP_U64(X) X
#define VIRTIOQ_SWAP_U32(X) X
#define VIRTIOQ_SWAP_U16(X) X

#endif

enum virtio_devices {
    reserved 			= 0,
    net 				= 1,
    blk 				= 2,
    console 			= 3,
    entropy 			= 4,
    balloon_traditional = 5,
    io_mem 				= 6,
    rpmsg 				= 7,
    scsi 				= 8,
    transport_9p 		= 9,
    mac80211 			= 10,
    rproc_serial 		= 11,
    virtio_caif 		= 12,
    balloon 			= 13,
    gpu 				= 16,
    timer 				= 17,
    input 				= 18
};

int virtio_device_device_ready(virtio_mmio_map* map);
int virtio_device_queue_add(virtio_mmio_map* map, u32 queue_n, struct virtq* queue);
int virtio_device_init(virtio_mmio_map* map,
                       enum virtio_devices device, u32 version, u32 vendor_id, u32 driver_features);

uint32_t virtio_device_get_status(virtio_mmio_map* map);
void virtio_device_ack_used(virtio_mmio_map* map);
void virtio_device_notify(virtio_mmio_map* map, u32 queue);

void virtio_q_add_descs(struct virtq* queue, le16 head);
void virtio_q_init_free(struct virtq* queue, le16* free_head, le16 start);
le16 virtio_q_alloc(struct virtq* queue, le16* free_head);
le16 virtio_q_free_length(struct virtq* queue, le16* free_head);
void virtio_q_free(struct virtq* queue, le16* free_head, le16 head, le16 tail);
int virtio_q_free_chain(struct virtq* queue, le16* free_head, le16 head);
int virtio_q_chain_add(struct virtq *queue, le16 *free_head, le16 *tail, le64 addr, le32 length, le16 flags);
int virtio_q_chain_add_virtual(struct virtq *queue, le16* free_head, le16 *tail, capability addr, le32 length, le16 flags);

#endif //CHERIOS_VIRTIO_H
