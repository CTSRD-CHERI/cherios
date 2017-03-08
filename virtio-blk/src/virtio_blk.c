/*-
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2016 Lawrence Esswood
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

#include "lib.h"

//FIXME this is broken for more than one user / a user who batches messages
//FIXME this needs a queue at the least, and we might get interrupt enable to take a handle to give back
static session_t * expected_session = NULL;
static size_t expected_i = 0;

static u32 mmio_read32(session_t* session, size_t offset) {
	return mips_cap_ioread_uint32(session->mmio_cap, offset);
}

static void mmio_write32(session_t* session, size_t offset, u32 value) {
	mips_cap_iowrite_uint32(session->mmio_cap, offset, value);
}

static void mmio_set32(session_t* session, size_t offset, u32 value) {
	value |= mmio_read32(session, offset);
	mips_cap_iowrite_uint32(session->mmio_cap, offset, value);
}

int vblk_init(session_t* session) {
	//printf(KBLU"%s\n"KRST, __func__);
	assert(session != NULL);

	/* INIT1: reset device */
	mmio_write32(session, VIRTIO_MMIO_STATUS, 0x0);
	session->init = 0;
	assert(mmio_read32(session, VIRTIO_MMIO_MAGIC_VALUE) == 0x74726976);	/* magic */
	assert(mmio_read32(session, VIRTIO_MMIO_VERSION) == 0x1);		/* legacy interface */
	assert(mmio_read32(session, VIRTIO_MMIO_DEVICE_ID) == 0x2);		/* block device */
	assert(mmio_read32(session, VIRTIO_MMIO_VENDOR_ID) == 0x554d4551);	/* vendor:QEMU */
	mmio_set32(session, VIRTIO_MMIO_STATUS, STATUS_ACKNOWLEDGE); /* INIT2: set ACKNOWLEDGE status bit */
	mmio_set32(session, VIRTIO_MMIO_STATUS, STATUS_DRIVER); /* INIT3: set DRIVER status bit */

	/* INIT4: select features */
	mmio_write32(session, VIRTIO_MMIO_HOST_FEATURES_SEL, 0x0);
	u32 device_features = mmio_read32(session, VIRTIO_MMIO_HOST_FEATURES);
	u32 driver_features = (1U << VIRTIO_BLK_F_GEOMETRY);
	mmio_write32(session, VIRTIO_MMIO_GUEST_FEATURES_SEL, 0x0);
	mmio_write32(session, VIRTIO_MMIO_GUEST_FEATURES, device_features&driver_features);

	/* INIT5 INIT6: legacy device, skipped */

	/* INIT7: set virtqueues */
	session->req_nb = 0x1;
	session->reqs = calloc(sizeof(req_t) * session->req_nb, 1);

	struct virtq * queue = &(session->queue);
	queue->num	= 3*session->req_nb;
	queue->desc	= calloc(16*queue->num, 1);
	queue->avail	= calloc(6+2*queue->num, 1);
	queue->used	= calloc(6+8*queue->num, 1);
	queue->last_used_idx = 0;

	assert(queue->desc != NULL);
	assert(queue->avail != NULL);
	assert(queue->used != NULL);

	queue->avail->flags = 0;
	queue->avail->idx = 0;

	mmio_write32(session, VIRTIO_MMIO_QUEUE_SEL, 0x0);
	assert(queue->num <= mmio_read32(session, VIRTIO_MMIO_QUEUE_NUM_MAX));
	mmio_write32(session, VIRTIO_MMIO_QUEUE_NUM, queue->num);
	mmio_write32(session, VIRTIO_MMIO_QUEUE_DESC_LOW,    (uint32_t)queue->desc);
	//mmio_write32(VIRTIO_MMIO_QUEUE_DESC_HIGH,  ((uint64_t)queue->desc) >> 32);
	mmio_write32(session, VIRTIO_MMIO_QUEUE_AVAIL_LOW,   (uint32_t)queue->avail);
	//mmio_write32(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, ((uint64_t)queue->avail) >> 32);
	mmio_write32(session, VIRTIO_MMIO_QUEUE_USED_LOW,    (uint32_t)queue->used);
	//mmio_write32(VIRTIO_MMIO_QUEUE_USED_HIGH,  ((uint64_t)queue->used) >> 32);

	mmio_write32(session, VIRTIO_MMIO_QUEUE_READY, 0x1);

	/* INIT8: set DRIVER_OK status bit */
	mmio_set32(session, VIRTIO_MMIO_STATUS, STATUS_DRIVER_OK);
	assert(!(mmio_read32(session, VIRTIO_MMIO_STATUS)&(STATUS_DEVICE_NEEDS_RESET)));

	session->init = 1;
	return 0;
}

int vblk_status(session_t* session) {
	//printf(KBLU"%s\n"KRST, __func__);
	if(session->init == 0) {
		return 1;
	}
	if(mmio_read32(session, VIRTIO_MMIO_STATUS) & STATUS_DEVICE_NEEDS_RESET) {
		return 1;
	}
	return 0;
}

size_t vblk_size(session_t* session) {
	//printf(KBLU"%s\n"KRST, __func__);
	struct virtio_blk_config * config =
	   (struct virtio_blk_config *)(session->mmio_cap + VIRTIO_MMIO_CONFIG);
	return config->capacity;
}

static int vblk_rw_ret(session_t* session, size_t i) {
	//printf(KBLU"%s\n"KRST, __func__);
	struct virtq * queue = &(session->queue);
	req_t * reqs = session->reqs;

	#if 0
	while(queue->used->idx == queue->last_used_idx) {
	}
	#endif

	//printf(KBLU"Used %x (%x)- %x %x\n"KRST, queue->used->idx, queue->last_used_idx, queue->used->ring[queue->last_used_idx % queue->num].id, queue->used->ring[queue->last_used_idx % queue->num].len);
	assert(queue->used->ring[queue->last_used_idx % queue->num].id == i);
	assert(queue->used->ring[queue->last_used_idx % queue->num].len > 0);
	assert(reqs[i].inhdr.status == VIRTIO_BLK_S_OK);
	queue->last_used_idx++;
	reqs[i].used = 0;

	/* ack used ring update */
	assert(mmio_read32(session, VIRTIO_MMIO_INTERRUPT_STATUS) == 0x1);
	mmio_write32(session, VIRTIO_MMIO_INTERRUPT_ACK, 0x1);
	assert(mmio_read32(session, VIRTIO_MMIO_INTERRUPT_STATUS) == 0x0);
	return 0;
}

int vblk_interrupt(void) {
	int ret = vblk_rw_ret(expected_session, expected_i);
	sync_state.sync_token = expected_session->reqs[expected_i].sync_token;
	sync_state.sync_caller = expected_session->reqs[expected_i].sync_caller;
	syscall_interrupt_enable(4);
	return ret;
}

static int vblk_send_done(session_t* session, size_t i) {
	expected_session = session;
	expected_i = i;
	sync_state.sync_token = NULL;
	sync_state.sync_caller = NULL;
	return 0;
}

int vblk_read(session_t* session, void * buf, size_t sector) {
	//printf(KBLU"%s\n"KRST, __func__);
	struct virtq * queue = &(session->queue);
	assert(!(mmio_read32(session, VIRTIO_MMIO_STATUS)&(STATUS_DEVICE_NEEDS_RESET)));

	/* find free request slot */
	req_t * reqs = session->reqs;
	size_t i;
	for(i=0; i<session->req_nb; i++) {
		if(reqs[i].used == 0) {
			break;
		}
	}
	assert(i < session->req_nb);
	reqs[i].used = 1;
	reqs[i].sync_token = sync_state.sync_token;
	reqs[i].sync_caller = sync_state.sync_caller;
	reqs[i].outhdr.type = VIRTIO_BLK_T_IN;
	reqs[i].outhdr.sector = sector;

	/* add it to virtqueue */
	queue->desc[3*i+0].addr = ((uint64_t)&(reqs[i].outhdr))&0xFFFFFFFF;
	queue->desc[3*i+0].len  = sizeof(struct virtio_blk_outhdr);
	queue->desc[3*i+0].flags  = VIRTQ_DESC_F_NEXT;
	queue->desc[3*i+0].next  = 1;

	queue->desc[3*i+1].addr = ((uint64_t)buf)&0xFFFFFFFF;
	queue->desc[3*i+1].len  = 512*sizeof(u8);
	queue->desc[3*i+1].flags  = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
	queue->desc[3*i+1].next  = 2;

	queue->desc[3*i+2].addr = ((uint64_t)&(reqs[i].inhdr))&0xFFFFFFFF;
	queue->desc[3*i+2].len  = sizeof(struct virtio_blk_inhdr);
	queue->desc[3*i+2].flags  = VIRTQ_DESC_F_WRITE;
	queue->desc[3*i+2].next  = 0;

	queue->avail->ring[queue->avail->idx] = 3*i;
	queue->avail->idx += 1;

	/* notify device */
	mmio_write32(session, VIRTIO_MMIO_QUEUE_NOTIFY, 0x0);

	//return vblk_rw_ret(i);
	return vblk_send_done(session, i);
}

int vblk_write(session_t* session, void * buf, size_t sector) {
	//printf(KBLU"%s\n"KRST, __func__);
	struct virtq * queue = &(session->queue);
	assert(!(mmio_read32(session, VIRTIO_MMIO_STATUS)&(STATUS_DEVICE_NEEDS_RESET)));

	/* find free request slot */
	req_t * reqs = session->reqs;
	size_t i;
	for(i=0; i<session->req_nb; i++) {
		if(reqs[i].used == 0) {
			break;
		}
	}
	assert(i < session->req_nb);
	reqs[i].used = 1;
	reqs[i].sync_token = sync_state.sync_token;
	reqs[i].sync_caller = sync_state.sync_caller;
	reqs[i].outhdr.type = VIRTIO_BLK_T_OUT;
	reqs[i].outhdr.sector = sector;

	/* add it to virtqueue */
	queue->desc[3*i+0].addr = ((uint64_t)&(reqs[i].outhdr))&0xFFFFFFFF;
	queue->desc[3*i+0].len  = sizeof(struct virtio_blk_outhdr);
	queue->desc[3*i+0].flags  = VIRTQ_DESC_F_NEXT;
	queue->desc[3*i+0].next  = 1;

	queue->desc[3*i+1].addr = ((uint64_t)buf)&0xFFFFFFFF;
	queue->desc[3*i+1].len  = 512*sizeof(u8);
	queue->desc[3*i+1].flags  = VIRTQ_DESC_F_NEXT;
	queue->desc[3*i+1].next  = 2;

	queue->desc[3*i+2].addr = ((uint64_t)&(reqs[i].inhdr))&0xFFFFFFFF;
	queue->desc[3*i+2].len  = sizeof(struct virtio_blk_inhdr);
	queue->desc[3*i+2].flags  = VIRTQ_DESC_F_WRITE;
	queue->desc[3*i+2].next  = 0;

	queue->avail->ring[queue->avail->idx] = 3*i;
	queue->avail->idx += 1;

	/* notify device */
	mmio_write32(session, VIRTIO_MMIO_QUEUE_NOTIFY, 0x0);

	return vblk_send_done(session, i);
}
