/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "lib.h"
#include "malta_virtio_mmio.h"
#include "mman.h"
#include "stdio.h"
#include "syscalls.h"
#include "assert.h"
#include "stdlib.h"
#include "object.h"

#define PADDR_LO(X) ((uint32_t)(X))
#define PADDR_HI(X) (uint32_t)((((uint64_t)(X) >> 32)) & 0xFFFFFFFF)

#define SECTOR_SIZE         512
#define MAX_REQS            4
#define DESC_PER_REQ        4
#define QUEUE_SIZE          (MAX_REQS * DESC_PER_REQ)

capability session_sealer;

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

static session_t* unseal_session(void* sealed_session) {
	register_t type = cheri_getcursor(session_sealer);

	if(cheri_gettype(sealed_session) != type) return NULL;

	session_t* session = (session_t*)cheri_unseal(sealed_session, session_sealer);

	seassion_state_e state;

	ENUM_VMEM_SAFE_DEREFERENCE(&session->state, state, session_terminated);

	if(state != session_created) return NULL;

	return session;
}

static void* seal_session(session_t* session) {
	return cheri_seal(session, session_sealer);
}

void * new_session(void * mmio_cap) {
	if(!VCAP(mmio_cap, 0x200, VCAP_RW)) {
		CHERI_PRINT_CAP(mmio_cap);
		assert(0);
		return NULL;
	}
	session_t * session = (session_t*)malloc(sizeof(session_t));
	session->state = session_created;
	session->mmio_cap = mmio_cap;
	session->init = 0;

    struct virtq * queue = &(session->queue);

    session->req_nb = MAX_REQS;
    queue->num	= (QUEUE_SIZE);

    // This is never accessed via physical pointers
    size_t req_size = (sizeof(req_t) * session->req_nb);

    // These are, and are put in one page each to ensure no boundry is crossed
    size_t out_size = (sizeof(struct virtio_blk_outhdr) * session->req_nb);
    size_t in_size = (sizeof(struct virtio_blk_inhdr) * session->req_nb);
    size_t desc_size = (16*queue->num);
    size_t avail_size = (6+2*queue->num);
    size_t used_size = (6+8*queue->num);

    assert(out_size < MEM_REQUEST_MIN_REQUEST);
    assert(in_size < MEM_REQUEST_MIN_REQUEST);
    assert(desc_size < MEM_REQUEST_MIN_REQUEST);
    assert(desc_size < MEM_REQUEST_MIN_REQUEST);
    assert(avail_size < MEM_REQUEST_MIN_REQUEST);
    assert(used_size < MEM_REQUEST_MIN_REQUEST);

    // TODO if possible we could put these inside one page
    // These are allocated in a way such that the structs will never cross a physical page boundry
    session->reqs = (req_t*)malloc(req_size);

    cap_pair pair;
#define GET_A_PAGE (rescap_take(mem_request(0, MEM_REQUEST_MIN_REQUEST, NONE, own_mop).val, &pair), pair.data)

    session->outhdrs = (struct virtio_blk_outhdr*)GET_A_PAGE;
    session->inhdrs = (struct virtio_blk_inhdr*)GET_A_PAGE;
    queue->desc	= (struct virtq_desc*)GET_A_PAGE;
    queue->avail = (struct virtq_avail*)GET_A_PAGE;
    queue->used = (struct virtq_used*)GET_A_PAGE;

    session->outhdrs_phy = mem_paddr_for_vaddr((size_t)session->outhdrs);
    session->inhdrs_phy = mem_paddr_for_vaddr((size_t)session->inhdrs);
    session->desc_phy = mem_paddr_for_vaddr((size_t)session->queue.desc);
    session->avail_phy = mem_paddr_for_vaddr((size_t)session->queue.avail);
    session->used_phy = mem_paddr_for_vaddr((size_t)session->queue.used);

    session->init = 0;
    capability sealed = seal_session(session);

    // Most of the setup is the same every time. We can fill the in/out blocks now
    for(le16 i = 0; i != session->req_nb; i++) {
        queue->desc[DESC_PER_REQ*i+0].addr = session->outhdrs_phy + (i * sizeof(struct virtio_blk_outhdr));
        queue->desc[DESC_PER_REQ*i+0].len  = sizeof(struct virtio_blk_outhdr);
        queue->desc[DESC_PER_REQ*i+0].flags  = VIRTQ_DESC_F_NEXT;
        queue->desc[DESC_PER_REQ*i+0].next  = DESC_PER_REQ*i + 1;

        queue->desc[DESC_PER_REQ*i+2].next = DESC_PER_REQ*i + 3;

        queue->desc[DESC_PER_REQ*i+3].addr = session->inhdrs_phy + (i * sizeof(struct virtio_blk_inhdr));
        queue->desc[DESC_PER_REQ*i+3].len  = sizeof(struct virtio_blk_inhdr);
        queue->desc[DESC_PER_REQ*i+3].flags  = VIRTQ_DESC_F_WRITE;
        queue->desc[DESC_PER_REQ*i+3].next  = 0;
    }



    int res = syscall_interrupt_register(VIRTIO_MMIO_IRQ, act_self_ctrl, -3, 0, sealed);
    assert_int_ex(res, == , 0);
    res = syscall_interrupt_enable(VIRTIO_MMIO_IRQ, act_self_ctrl);
    assert_int_ex(res, == , 0);

	return sealed;
}



int vblk_init(session_t* session) {
    // printf(KBLU"%s\n"KRST, __func__);

	session = unseal_session(session);
	assert(session != NULL);

	/* INIT1: reset device */
	mmio_write32(session, VIRTIO_MMIO_STATUS, 0x0);
	session->init = 0;
	assert_int_ex(mmio_read32(session, VIRTIO_MMIO_MAGIC_VALUE), ==, 0x74726976);	/* magic */
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

	struct virtq * queue = &(session->queue);

    // If init is called again just reset but don't allocate new buffers

	assert(queue->desc != NULL);
	assert(queue->avail != NULL);
	assert(queue->used != NULL);

	queue->avail->flags = 0;
	queue->avail->idx = 0;
    queue->used->idx = 0;
    queue->last_used_idx = 0;

	mmio_write32(session, VIRTIO_MMIO_QUEUE_SEL, 0x0);
	assert(queue->num <= mmio_read32(session, VIRTIO_MMIO_QUEUE_NUM_MAX));
	mmio_write32(session, VIRTIO_MMIO_QUEUE_NUM, queue->num);
	mmio_write32(session, VIRTIO_MMIO_QUEUE_DESC_LOW,    PADDR_LO(session->desc_phy));
	mmio_write32(session, VIRTIO_MMIO_QUEUE_DESC_HIGH,   PADDR_HI(session->desc_phy));
	mmio_write32(session, VIRTIO_MMIO_QUEUE_AVAIL_LOW,   PADDR_LO(session->avail_phy));
	mmio_write32(session, VIRTIO_MMIO_QUEUE_AVAIL_HIGH,  PADDR_HI(session->avail_phy));
	mmio_write32(session, VIRTIO_MMIO_QUEUE_USED_LOW,    PADDR_LO(session->used_phy));
	mmio_write32(session, VIRTIO_MMIO_QUEUE_USED_HIGH,   PADDR_HI(session->used_phy));

	mmio_write32(session, VIRTIO_MMIO_QUEUE_READY, 0x1);

	/* INIT8: set DRIVER_OK status bit */
	mmio_set32(session, VIRTIO_MMIO_STATUS, STATUS_DRIVER_OK);
	assert(!(mmio_read32(session, VIRTIO_MMIO_STATUS)&(STATUS_DEVICE_NEEDS_RESET)));

	session->init = 1;
	return 0;
}

int vblk_status(session_t* session) {
	session = unseal_session(session);
    assert(session);

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
	session = unseal_session(session);
    assert(session);

	//printf(KBLU"%s\n"KRST, __func__);
	struct virtio_blk_config * config =
	   (struct virtio_blk_config *)(session->mmio_cap + VIRTIO_MMIO_CONFIG);
	return config->capacity;
}

static void vblk_send_result(req_t* req, int result) {
    if(req->sync_token) {
        message_reply(NULL, (register_t)result, 0, req->caller.sync_caller, req->sync_token);
    } else if(req->caller.sync_caller) {
        // TODO set seq response
        message_send((register_t)result,req->seq_response,0,0,NULL,NULL,NULL,NULL,
                     req->caller.sync_caller, SEND, req->seq_port);
    }
}

static void vblk_rw_ret(session_t* session) {
	//printf(KBLU"%s\n"KRST, __func__);
	struct virtq * queue = &(session->queue);
	req_t * reqs = session->reqs;

    /* Process everything since last used to used index */
    for(le16 ndx = queue->last_used_idx; ndx != queue->used->idx; ndx++) {

        le16 i = (queue->used->ring[ndx % queue->num].id / DESC_PER_REQ);

        le32 used_desc_id =  queue->used->ring[ndx % queue->num].id;
        le32 used_len = queue->used->ring[ndx % queue->num].len;

        //printf(KBLU"Used %x (%x)- %x %x\n"KRST, queue->used->idx, queue->last_used_idx, used_desc_id, used_len);

        assert_int_ex(used_desc_id, ==, DESC_PER_REQ * i);
        assert_int_ex(used_len, >, 0);
        assert_int_ex(session->inhdrs[i].status, ==, VIRTIO_BLK_S_OK);

        vblk_send_result(reqs+i, 0);

        queue->last_used_idx++;
        reqs[i].used = 0;
    }

    /* ack used ring update */
    if(mmio_read32(session, VIRTIO_MMIO_INTERRUPT_STATUS) == 0x1) {
        mmio_write32(session, VIRTIO_MMIO_INTERRUPT_ACK, 0x1);
        assert(mmio_read32(session, VIRTIO_MMIO_INTERRUPT_STATUS) == 0x0);
    }
}

void vblk_interrupt(void* sealed_session, register_t a0, register_t irq) {
    vblk_rw_ret(unseal_session(sealed_session));
	syscall_interrupt_enable((int)irq, act_self_ctrl);
}

static int vblk_delay_return(session_t* session, size_t i) {
	sync_state.sync_token = NULL;
	sync_state.sync_caller = NULL;
	return 0;
}

int vblk_rw(session_t* session, void * buf, size_t sector,
             act_kt async_caller, register_t asyc_no, register_t async_port,
             __virtio32 type) {
    //printf(KBLU"%s\n"KRST, __func__);

    session = unseal_session(session);
    assert(session);
    assert(session->init);

    struct virtq * queue = &(session->queue);
    assert(!(mmio_read32(session, VIRTIO_MMIO_STATUS)&(STATUS_DEVICE_NEEDS_RESET)));

    /* find free request slot */
    le16 i;
    for(i=0; i<session->req_nb; i++) {
        if(session->reqs[i].used == 0) {
            break;
        }
    }

    assert(i != session->req_nb);

    req_t * reqs = session->reqs;
    struct virtio_blk_outhdr* outhdr = &session->outhdrs[i];

    reqs[i].used = 1;
    if(async_caller) {
        reqs[i].sync_token = NULL;
        reqs[i].caller.async_caller = async_caller;
        reqs[i].seq_response = asyc_no;
        reqs[i].seq_port = async_port;

    } else {
        reqs[i].sync_token = sync_state.sync_token;
        reqs[i].caller.sync_caller = sync_state.sync_caller;
    }
    outhdr->type = type;
    outhdr->sector = sector;
    le16 flag_type = type == VIRTIO_BLK_T_IN ? ( VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT) : VIRTQ_DESC_F_NEXT;

    size_t paddr_start = mem_paddr_for_vaddr((size_t)buf);
    size_t paddr_end = mem_paddr_for_vaddr((size_t)buf + (SECTOR_SIZE - 1));
    int one_piece = (paddr_end - paddr_start) == (SECTOR_SIZE - 1);

    /* The IN/OUT headers are already set up in init. We only need to add in the address of the users buffer */

    /* This sets up for a r/w, the buffer may be split in two parts if it crosses a physical page boundry
     * It cannot cross more than one as it is only of length SECTOR_SIZE*/
    queue->desc[DESC_PER_REQ*i+1].addr = paddr_start;
    queue->desc[DESC_PER_REQ*i+1].flags  = flag_type;
    if(one_piece) {
        queue->desc[DESC_PER_REQ*i+1].next  = (DESC_PER_REQ*i) + 3;
        queue->desc[DESC_PER_REQ*i+1].len  = SECTOR_SIZE*sizeof(u8);
    } else {
        size_t len_1 = PHY_PAGE_SIZE - (paddr_start & (PHY_PAGE_SIZE-1));
        size_t len_2 = SECTOR_SIZE - len_1;
        size_t paddr_2 = paddr_end & ~(PHY_PAGE_SIZE-1);

        queue->desc[DESC_PER_REQ*i+1].next  = (DESC_PER_REQ*i) + 2;
        queue->desc[DESC_PER_REQ*i+1].len  = len_1*sizeof(u8);

        queue->desc[DESC_PER_REQ*i+2].addr  = paddr_2;
        queue->desc[DESC_PER_REQ*i+2].flags  = flag_type;
        queue->desc[DESC_PER_REQ*i+2].len  = len_2*sizeof(u8);
    }

    queue->avail->ring[queue->avail->idx % DESC_PER_REQ] = (le16)DESC_PER_REQ*i;
    queue->avail->idx += 1;

    /* notify device */
    mmio_write32(session, VIRTIO_MMIO_QUEUE_NOTIFY, 0x0);

    //return vblk_rw_ret(i);
    return vblk_delay_return(session, i);
}

int vblk_read(session_t* session, void * buf, size_t sector,
              act_kt async_caller, register_t asyc_no, register_t async_port) {
    return vblk_rw(session, buf, sector, async_caller, asyc_no, async_port, VIRTIO_BLK_T_IN);
}

int vblk_write(session_t* session, void * buf, size_t sector
        , act_kt async_caller, register_t asyc_no, register_t async_port) {
    return vblk_rw(session, buf, sector, async_caller, asyc_no, async_port, VIRTIO_BLK_T_OUT);
}
