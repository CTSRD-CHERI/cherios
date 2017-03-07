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

#include "mips.h"
#include "sys/mman.h"
#include "object.h"
#include "namespace.h"
#include "stdio.h"
#include "assert.h"

static act_kt memmgt_ref = NULL;

static void *_mmap(void *addr, size_t length, int prot, int flags) {
	if(memmgt_ref == NULL) {
		memmgt_ref = namespace_get_ref(namespace_num_memmgt);
		assert(memmgt_ref != NULL);
	}
	return MESSAGE_SYNC_SEND_c(memmgt_ref, length, prot, flags, addr, NULL, NULL, 0);
}

void *mmap(void *addr, size_t length, int prot, int flags, __unused int fd, __unused off_t offset) {
	return _mmap(addr, length, prot, flags);
}

int munmap(void *addr, size_t length) {
	return MESSAGE_SYNC_SEND_r(memmgt_ref, length, 0, 0, addr, NULL, NULL, 1);
}

void mmap_set_act(act_kt ref) {
	memmgt_ref = ref;
}
