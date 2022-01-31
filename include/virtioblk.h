/*-
 * Copyright (c) 2016 Hadrien Barral
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

#ifndef _VIRTIO_BLK_H
#define _VIRTIO_BLK_H

#include "object.h"
#include "namespace.h"
#include "assert.h"
#include "cheric.h"
#include "sockets.h"
#include "aes.h"

typedef struct block_aes_data_s {
	struct AES_ctx ctx;
	const uint8_t* key;
	uint8_t iv[AES_BLOCKLEN];
	capability check_arg;
	int innited;
} block_aes_data_t;

typedef enum {
	REQUEST_SET_KEY = REQUEST_OUT_USER_START,
} request_type_oob_bc_e;

extern void * vblk_ref;
extern capability virt_session;

static inline void virtio_check_refs(void) {
	if(vblk_ref == NULL) {
		vblk_ref = namespace_get_ref(namespace_num_blockcache);
	}
	assert(vblk_ref != NULL);
}


static inline
MESSAGE_WRAP_ID_ASSERT(int, virtio_new_socket_dont_connect, (requester_t, requester, enum socket_connect_type, type), vblk_ref, 5, namespace_num_blockcache, virt_session)

static inline int virtio_new_socket(requester_t requester, enum socket_connect_type type) {
	int res = virtio_new_socket_dont_connect(requester, type);

	if(res == 0) {
		socket_requester_connect(requester);
	}

	return res;
}

static inline void virtio_blk_session(void * mmio_cap) {
	virtio_check_refs();
	virt_session = message_send_c(MARSHALL_ARGUMENTS(mmio_cap), vblk_ref, SYNC_CALL, -1);
}

static inline
MESSAGE_WRAP_ID_ASSERT(int, virtio_blk_init, (void), vblk_ref, 0, namespace_num_blockcache, virt_session)

static inline
MESSAGE_WRAP_ID_ASSERT(int, virtio_read, (void *, buf, size_t, sector), vblk_ref, 1, namespace_num_blockcache, virt_session)

static inline
MESSAGE_WRAP_ASYNC_ID_ASSERT(virtio_async_read, (void*, buf, size_t, sector, register_t, async_no, register_t, async_port), vblk_ref, 1, namespace_num_blockcache)

static inline
MESSAGE_WRAP_ID_ASSERT(int, virtio_write, (const void *, buf, size_t, sector), vblk_ref, 2, namespace_num_blockcache, virt_session)

static inline
MESSAGE_WRAP_ASYNC_ID_ASSERT(virtio_async_write, (const void*, buf, size_t, sector, register_t, async_no, register_t, async_port), vblk_ref, 2, namespace_num_blockcache)

static inline
MESSAGE_WRAP_ID_ASSERT(int, virtio_blk_status, (void), vblk_ref, 3, namespace_num_blockcache, virt_session)

static inline
MESSAGE_WRAP_ID_ASSERT(size_t, virtio_blk_size, (void), vblk_ref, 4, namespace_num_blockcache, virt_session)

static inline
MESSAGE_WRAP_ID_ASSERT(void, virtio_writeback_all, (void), vblk_ref, 6, namespace_num_blockcache, virt_session)


#endif // _VIRTIO_BLK_H
