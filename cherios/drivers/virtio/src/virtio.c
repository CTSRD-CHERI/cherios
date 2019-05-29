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

#include "virtio.h"
#include "cheric.h"
#include "mman.h"
#include "assert.h"
#include "stdio.h"

#define PADDR_LO(X) ((uint32_t)(X))
#define PADDR_HI(X) (uint32_t)((((uint64_t)(X) >> 32)) & 0xFFFFFFFF)

#define TOUCH(X) *((volatile char*)(X)) = *((volatile char*)(X))

int virtio_device_device_ready(virtio_mmio_map* map) {

    map->status |= STATUS_DRIVER_OK;
    map->status |= STATUS_FEATURES_OK;
    if(map->status & STATUS_DEVICE_NEEDS_RESET) return DRIVER_DEVICE_NEEDS_RESET;

    return 0;
}

int virtio_device_queue_add(virtio_mmio_map* map, u32 queue_n, struct virtq* queue) {
    /* INIT7: set virtqueues */

    if(queue_n >= map->queue_num_max)

    if(queue->desc == NULL) return DRIVER_QUEUE_MISSING_FIELDS;
    if(queue->avail == NULL) return DRIVER_QUEUE_MISSING_FIELDS;
    if(queue->used == NULL) return DRIVER_QUEUE_MISSING_FIELDS;

    queue->avail->flags = 0;
    queue->avail->idx = 0;
    queue->used->idx = 0;
    queue->last_used_idx = 0;

    map->queue_sel = queue_n;
    if(queue->num > map->queue_num_max) return DRIVER_QUEUE_TOO_LONG;
    map->queue_num = queue->num;

#define P_FOR(X) 								\
        TOUCH(queue-> X); 						\
		size_t X ## _sz = X ## _size(queue);	\
		uint64_t X ## _phy = translate_address((size_t)queue-> X, 0); 							\
		uint64_t X ## _phy_end = translate_address(((size_t)queue-> X) + X ## _sz -1, 0); 		\
		if(X ## _phy + X ## _sz - 1 != X ## _phy_end) return DRIVER_QUEUE_CROSSES_PAGE_BOUNDRY;	\
		map->queue_ ## X ## _low = PADDR_LO(X ## _phy);												\
		map->queue_ ## X ## _high = PADDR_HI(X ## _phy);

    P_FOR(desc);
    P_FOR(avail);
    P_FOR(used);

    map->queue_ready = 1;

    return 0;
}
int virtio_device_init(virtio_mmio_map* map,
                       enum virtio_devices device, u32 version, u32 vendor_id, u32 driver_features) {
    /* INIT1: reset device */
    map->status = 0;

    // Check device is what we expect
    if(map->magic_value != 0x74726976) return DRIVER_BAD_DEVICE;
    if(map->version != version) return DRIVER_BAD_DEVICE;
    if(map->device_id != (u32)device) return DRIVER_BAD_DEVICE;
    if(map->vendor_id != vendor_id) return DRIVER_BAD_DEVICE;

    /* INIT2: set ACKNOWLEDGE status bit */
    /* INIT3: set DRIVER status bit */

    map->status |= STATUS_ACKNOWLEDGE | STATUS_DRIVER;

    /* INIT4: select features */
    map->host_features_sel = 0;
    u32 device_features = map->host_features;
    printf("Device %d has features %x\n", device, device_features);
    map->guest_features_sel = 0;
    map->guest_features = device_features & driver_features;

    if((device_features & driver_features) != driver_features) return DRIVER_BAD_FEATURES;

    /* INIT5 INIT6: legacy device, skipped */

    return 0;
}

void virtio_device_ack_used(virtio_mmio_map* map) {
    //if(map->interrupt_status == 1) {
        map->interrupt_ack = 1;
    //}
}


void virtio_device_notify(virtio_mmio_map* map, u32 queue) {
    HW_SYNC;
    map->queue_notify = queue;
}

void virtio_q_add_descs(struct virtq* queue, le16 head) {
    le16 ndx = queue->avail->idx;
    queue->avail->ring[ndx % queue->num] = head;
    HW_SYNC;
    queue->avail->idx = ndx+1;
    return;
}

void virtio_q_init_free(struct virtq* queue, le16* free_head, le16 start) {
    for(le16 i = start; i != queue->num; i++) {
        queue->desc[i].next = i+1;
    }
    *free_head = start;
}

le16 virtio_q_alloc(struct virtq* queue, le16* free_head) {
    le16 head = *free_head;
    if(head != queue->num) {
        *free_head = queue->desc[head].next;
    }
    return head;
}

le16 virtio_q_free_length(struct virtq* queue, le16* free_head) {
    le16 num = 0;
    le16 head = *free_head;
    while(head != queue->num) {
        head = queue->desc[head].next;
        num++;
    }
    return num;
}

void virtio_q_free(struct virtq* queue, le16* free_head, le16 head, le16 tail) {
    queue->desc[tail].next = *free_head;
    *free_head = head;
}

int virtio_q_free_chain(struct virtq* queue, le16* free_head, le16 head) {
    le16 tail = head;
    int num = 1;
    while(queue->desc[tail].flags & VIRTQ_DESC_F_NEXT) {
        tail = queue->desc[tail].next;
        num++;
    }
    virtio_q_free(queue, free_head, head, tail);
    return num;
}

int virtio_q_chain_add(struct virtq *queue, le16 *free_head, le16 *tail, le64 addr, le32 length, le16 flags) {
    le16 new = virtio_q_alloc(queue, free_head);
    if(new == queue->num) return -1;
    queue->desc[*tail].next = new;
    queue->desc[*tail].flags |= VIRTQ_DESC_F_NEXT;

    *tail = new;
    struct virtq_desc* desc = queue->desc + new;
    desc->len = length;
    desc->addr = addr;
    desc->flags = flags;

    return 0;
}

struct virtq_arg {
    struct virtq *queue;
    le16* free_head;
    le16 *tail;
    le16 flags;
};

int virtio_phy_handle_func(capability arg, __unused phy_handle_flags flags, size_t phy_addr, size_t length) {
    struct virtq_arg* virtq_args = (struct virtq_arg*)arg;
    assert_int_ex(phy_addr, >=, 0x1000);
    int res = virtio_q_chain_add(virtq_args->queue, virtq_args->free_head, virtq_args->tail, phy_addr, (le16)length, virtq_args->flags);
    return res;
}

int virtio_q_chain_add_virtual(struct virtq *queue, le16* free_head, le16 *tail, capability addr, le32 length, le16 flags) {
    struct virtq_arg arg;
    arg.flags = flags;
    arg.queue = queue;
    arg.free_head = free_head;
    arg.tail = tail;
    return for_each_phy((capability)&arg, PHY_HANDLE_NONE, &virtio_phy_handle_func, (char*)addr, length);
}