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
#include "sockets.h"
#include "virtio.h"

#define SECTOR_SIZE         512
#define MAX_REQS            4
#define DESC_PER_REQ        4

#define FLEX_QUEUE_SIZE     0x100
#define SIMPLE_QUEUE_SIZE   (MAX_REQS * DESC_PER_REQ)
#define QUEUE_SIZE          (FLEX_QUEUE_SIZE + SIMPLE_QUEUE_SIZE)

capability session_sealer;

size_t n_socks = 0;

struct session_sock {
    session_t* session;
    uni_dir_socket_fulfiller ff;
    uint16_t in_sector_prog;
    le16 req_tail;
    le16 req_head;
    le16 mid_flag_type;
    size_t out_paddr;
    size_t in_paddr;
    struct virtio_blk_outhdr out;
    struct virtio_blk_inhdr in;

} socks[VIRTIO_MAX_SOCKS];

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
    size_t desc_size = desc_size(queue);
    size_t avail_size = avail_size(queue);
    size_t used_size = used_size(queue);

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

    session->outhdrs_phy = translate_address((size_t)session->outhdrs, 0);
    session->inhdrs_phy = translate_address((size_t)session->inhdrs, 0);

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

    // Create a free chain out of the flexible entries

    virtio_q_init_free(queue, &session->free_head, SIMPLE_QUEUE_SIZE);

    int res = syscall_interrupt_register(VIRTIO_MMIO_IRQ, act_self_ctrl, -3, 0, sealed);
    assert_int_ex(res, == , 0);
    res = syscall_interrupt_enable(VIRTIO_MMIO_IRQ, act_self_ctrl);
    assert_int_ex(res, == , 0);

	return sealed;
}

static void add_desc(session_t* session, le16 desc_no) {
    virtio_q_add_descs(&session->queue, desc_no);
    virtio_device_notify((virtio_mmio_map*)session->mmio_cap, 0);
}

static ssize_t full_oob(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length) {
    struct session_sock* ss = (struct session_sock*)arg;
    request_type_e req = request->type;

    if(req == REQUEST_SEEK) {
        assert(ss->in_sector_prog == 0);
        int64_t seek_offset = request->request.seek_desc.v.offset;
        int whence = request->request.seek_desc.v.whence;

        size_t target_offset;

        switch (whence) {
            case SEEK_CUR:
                target_offset = seek_offset + ss->out.sector;
                break;
            case SEEK_SET:
                if(seek_offset < 0) return E_OOB;
                target_offset = (size_t)seek_offset;
                break;
            case SEEK_END:
            default:
                return E_OOB;
        }

        ss->out.sector = target_offset;

        return length;
    } else if(req == REQUEST_FLUSH) {
        assert(0 && "TODO");
    }

    return E_OOB;
}

static ssize_t ful_ff(capability arg, char* buf, uint64_t offset, uint64_t length) {
    struct session_sock* ss = (struct session_sock*)arg;

    assert(length <= SECTOR_SIZE);

    int res = virtio_q_chain_add_virtual(&ss->session->queue, &ss->session->free_head, &ss->req_tail,
                               (capability)buf, (le32)length, ss->mid_flag_type);

    assert(res > 0 && "Out of descriptors");

    ss->in_sector_prog+=length;

    assert(ss->in_sector_prog <= SECTOR_SIZE);

    return length;
}

static void translate_sock(struct session_sock* ss) {
    uint64_t bytes = socket_internal_requester_bytes_requested(ss->ff.requester);

    ss->in_sector_prog = 0;

    if(bytes == 0) {
        // Just an oob
        ssize_t ret = socket_internal_fulfill_progress_bytes(&ss->ff, SOCK_INF,
                                                             F_CHECK | F_PROGRESS | F_DONT_WAIT,
                                                             &ful_func_cancel_non_oob, (capability)ss,0,full_oob, NULL);
        return;
    }

    assert_int_ex(bytes, >=, SECTOR_SIZE);

    ss->req_head = ss->req_tail = virtio_q_alloc(&ss->session->queue, &ss->session->free_head);

    assert(ss->req_head != ss->session->queue.num && "No descriptors");

    struct virtq_desc* desc_head = ss->session->queue.desc + ss->req_head;
    desc_head->len = sizeof(struct virtio_blk_outhdr);
    desc_head->addr = ss->out_paddr;
    desc_head->flags = VIRTQ_DESC_F_NEXT;

    // TODO can use checkpoint to allow queueing here
    ssize_t bytes_translated = socket_internal_fulfill_progress_bytes(&ss->ff, SECTOR_SIZE,
                                                                      F_CHECK | F_DONT_WAIT,
                                                                      ful_ff, (capability)ss,0,full_oob, NULL);
    assert_int_ex(bytes_translated, ==, SECTOR_SIZE);

    assert_int_ex(-bytes_translated, ==, -SECTOR_SIZE);
    assert(ss->in_sector_prog == SECTOR_SIZE);

    int res = virtio_q_chain_add(&ss->session->queue, &ss->session->free_head, &ss->req_tail,
                                 ss->in_paddr, (le16) sizeof(struct virtio_blk_inhdr), VIRTQ_DESC_F_WRITE);
    assert(res == 0 && "Out of descriptors");

    add_desc(ss->session, ss->req_head);
}

int new_socket(session_t* session, uni_dir_socket_requester* requester, enum socket_connect_type type) {
    session = unseal_session(session);
    assert(session != NULL);

    if(n_socks == VIRTIO_MAX_SOCKS) return -1;

    struct session_sock* sock = &socks[n_socks];

    uint8_t sock_type;
    if(type == CONNECT_PUSH_WRITE) {
        sock_type = SOCK_TYPE_PUSH;
    } else if(type == CONNECT_PULL_READ) {
        sock_type = SOCK_TYPE_PULL;
    } else return -1;

    ssize_t res;
    if((res = socket_internal_fulfiller_init(&sock->ff, sock_type)) < 0) return (int)res;
    if((res = socket_internal_fulfiller_connect(&sock->ff, requester)) < 0) return (int)res;

    sock->session = session;
    sock->out.ioprio = 0;
    sock->out.sector = 0;
    sock->out.type = (sock_type == SOCK_TYPE_PUSH) ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    sock->mid_flag_type = (sock_type == SOCK_TYPE_PUSH) ?  VIRTQ_DESC_F_NEXT : (VIRTQ_DESC_F_NEXT |  VIRTQ_DESC_F_WRITE);
    sock->in_sector_prog = 0;
    sock->req_head = sock->req_tail = QUEUE_SIZE;
    sock->out_paddr = translate_address((size_t)&sock->out, 0);
    size_t diff = sizeof(struct virtio_blk_outhdr) - 1;
    // Check we don't cross a page boundry. This is just really unlucky.
    assert(translate_address((size_t)&sock->out + diff, 0) - diff == sock->out_paddr);
    sock->in_paddr = translate_address((size_t)&sock->in, 0);

    n_socks++;

    return 0;
}

void handle_loop(void) {

    int anything;
    int set_waiting = 0;

    while(1) {

        if(!msg_queue_empty()) {
            msg_entry(1);
        }

        // Try to avoid late notifies from last poll
        if(set_waiting) syscall_cond_cancel();

        anything = 0;

        for(size_t i = 0; i < n_socks;i++) {
            if(socks[i].req_head == QUEUE_SIZE) {
                enum poll_events event = socket_internal_fulfill_poll(&socks[i].ff, POLL_IN, set_waiting, 0);
                if(event) {
                    if(event & (POLL_HUP | POLL_ER | POLL_NVAL)) assert(0 && "Socket error in block device");
                    translate_sock(socks+i);
                    anything = 1;
                    set_waiting = 0;
                }
            }
        }

        if(!anything) {
            if(set_waiting) syscall_cond_wait(1, 0);
            // On the next loop we will set up notifications on the sockets
            set_waiting = 1;
        }
    }
}

int vblk_init(session_t* session) {
    // printf(KBLU"%s\n"KRST, __func__);

	session = unseal_session(session);
	assert(session != NULL);
    struct virtq * queue = &(session->queue);
    session->init = 0;

    int result = virtio_device_init((virtio_mmio_map*)session->mmio_cap, blk, 0x1, VIRTIO_QEMU_VENDOR, (1U << VIRTIO_BLK_F_GEOMETRY));
    assert_int_ex(-result, ==, 0);
    result = virtio_device_queue_add((virtio_mmio_map*)session->mmio_cap, 0, queue);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_device_ready((virtio_mmio_map*)session->mmio_cap);
    assert_int_ex(-result, ==, 0);
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
    if(req->sync_caller.sync_caller) {
        msg_resume_return(NULL, (register_t)result, 0, req->sync_caller);
    } else if(req->async_caller) {
        // TODO set seq response
        message_send((register_t)result,req->seq_response,0,0,NULL,NULL,NULL,NULL,
                     req->async_caller, SEND, req->seq_port);
    }
}

static void vblk_rw_ret(session_t* session) {
	//printf(KBLU"%s\n"KRST, __func__);
	struct virtq * queue = &(session->queue);
	req_t * reqs = session->reqs;

    /* Process everything since last used to used index */
    for(le16 ndx = queue->last_used_idx; ndx != queue->used->idx; ndx++) {
        le32 used_desc_id =  queue->used->ring[ndx % queue->num].id;
        le32 used_len = queue->used->ring[ndx % queue->num].len;
        //printf(KMAJ"Used %x (%x)- %x %x\n"KRST, queue->used->idx, queue->last_used_idx, used_desc_id, used_len);

        if(used_desc_id < SIMPLE_QUEUE_SIZE) { // simple directly mapped
            le16 i = (used_desc_id / DESC_PER_REQ);
            assert_int_ex(used_desc_id, ==, DESC_PER_REQ * i);
            assert_int_ex(used_len, >, 0);
            assert_int_ex(session->inhdrs[i].status, ==, VIRTIO_BLK_S_OK);

            vblk_send_result(reqs+i, 0);

            reqs[i].used = 0;
        } else { // more complex - requires us to find out which socket this came from
            for(size_t i = 0; i < n_socks; i++) {
                if(socks[i].req_head == used_desc_id) {
                    // This sockets request has finished
                    ssize_t ret = socket_internal_fulfill_progress_bytes(&socks[i].ff, SECTOR_SIZE, F_PROGRESS | F_DONT_WAIT,
                                                                         NULL, NULL, 0, ful_oob_func_skip_oob, NULL);
                    assert_int_ex(-ret, ==, -SECTOR_SIZE);
                    assert_int_ex(socks[i].in.status, ==, VIRTIO_BLK_S_OK);
                    virtio_q_free(&socks[i].session->queue, &session->free_head, socks[i].req_head, socks[i].req_tail);
                    socks[i].req_head = QUEUE_SIZE;
                    socks[i].out.sector++;
                    break;
                }
            }
        }

        queue->last_used_idx++;
    }

    /* ack used ring update */
    virtio_device_ack_used((virtio_mmio_map*)session->mmio_cap);
}

void vblk_interrupt(void* sealed_session, register_t a0, register_t irq) {
    vblk_rw_ret(unseal_session(sealed_session));
	syscall_interrupt_enable((int)irq, act_self_ctrl);
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

    reqs[i].async_caller = async_caller;

    if(async_caller) {
        reqs[i].sync_caller.sync_caller = NULL;
        reqs[i].seq_response = asyc_no;
        reqs[i].seq_port = async_port;
    } else {
        msg_delay_return(&reqs[i].sync_caller);
    }
    outhdr->type = type;
    outhdr->sector = sector;
    le16 flag_type = type == VIRTIO_BLK_T_IN ? ( VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT) : VIRTQ_DESC_F_NEXT;

    size_t paddr_start = translate_address((size_t)buf, 0);
    size_t paddr_end = translate_address((size_t)buf + (SECTOR_SIZE - 1), 0);
    int one_piece = (paddr_end - paddr_start) == (SECTOR_SIZE - 1);

    assert_int_ex(paddr_start, !=, (size_t)-1);
    assert_int_ex(paddr_end, !=, (size_t)-1);

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

    add_desc(session, (le16)DESC_PER_REQ*i);

    return 0;
}

int vblk_read(session_t* session, void * buf, size_t sector,
              act_kt async_caller, register_t asyc_no, register_t async_port) {
    return vblk_rw(session, buf, sector, async_caller, asyc_no, async_port, VIRTIO_BLK_T_IN);
}

int vblk_write(session_t* session, void * buf, size_t sector
        , act_kt async_caller, register_t asyc_no, register_t async_port) {
    return vblk_rw(session, buf, sector, async_caller, asyc_no, async_port, VIRTIO_BLK_T_OUT);
}
