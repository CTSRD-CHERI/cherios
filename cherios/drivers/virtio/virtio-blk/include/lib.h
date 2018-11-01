/*-
 * Copyright (c) 2016 Hadrien Barral
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
 */

#ifndef VIRTIO_LIB
#define VIRTIO_LIB

#include "cheric.h"
#include "virtio_blk.h"
#include "virtio_mmio.h"
#include "virtio_queue.h"
#include "types.h"
#include "nano/nanotypes.h"
#include "sockets.h"

#define VIRTIO_MAX_SOCKS 8

// This request structure also needs an inhdr and outhdr, but they are kept seperate for alignment
typedef struct req_s {
    uint8_t used;
    register_t seq_response;
	register_t seq_port;
	sync_state_t sync_caller;
    act_kt async_caller;
} req_t;

typedef enum {
	session_created = 0,
	session_terminated = 1
} seassion_state_e;

typedef struct session_s {
	seassion_state_e state;
	char * mmio_cap;
	int init;
	struct virtq queue;
	size_t req_nb;

    le16 free_head;

	// Physical start address' for everything in the virtq and hdrs
	size_t outhdrs_phy;
	size_t inhdrs_phy;
	size_t desc_phy;
	size_t avail_phy;
	size_t used_phy;

	// 1 to 1 but split for better alignment of things that need physical addressing
	req_t * 	reqs;
	struct virtio_blk_outhdr* outhdrs; 		// 16 bytes each
	struct virtio_blk_inhdr*  inhdrs;		// 1 byte
} session_t;

capability session_sealer;

void mmio_disk(void*);

void handle_loop(void);
void * new_session(void * mmio_cap);
int new_socket(session_t* session, uni_dir_socket_requester* requester, enum socket_connect_type);
int	vblk_init(session_t* session);
int	vblk_read(session_t* session, void * buf, size_t sector, act_kt async_caller, register_t asyc_no, register_t async_port);
int	vblk_write(session_t* session, void * buf, size_t sector, act_kt async_caller, register_t asyc_no, register_t async_port);
int	vblk_status(session_t* session);
size_t	vblk_size(session_t* session);

void vblk_interrupt(void* sealed_session, register_t a0, register_t irq);

#endif // VIRTIO_LIB