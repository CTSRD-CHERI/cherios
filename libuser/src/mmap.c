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

static int _mmap(size_t base, size_t length, int cheri_perms, int flags, cap_pair* result) {
	if(memmgt_ref == NULL) {
		memmgt_ref = namespace_get_ref(namespace_num_memmgt);
		assert(memmgt_ref != NULL);
	}
	return MESSAGE_SYNC_SEND_r(memmgt_ref, base, length, cheri_perms, flags, result, NULL, NULL, NULL, 0);
}

void *mmap(void *addr, size_t length, int prot, int flags, __unused int fd, __unused off_t offset) {
	cap_pair pair;

	assert(addr == NULL && "The old interface only supports an addr of null");

	if((prot & PROT_EXECUTE) && (prot & PROT_WRITE)) {
		assert(0 && "The old interface cannot return a single capability with both execute and write privs");
	}

	int perms = CHERI_PERM_ALL &
				~(CHERI_PERM_EXECUTE|CHERI_PERM_LOAD|CHERI_PERM_STORE
				  |CHERI_PERM_LOAD_CAP|CHERI_PERM_STORE_CAP|CHERI_PERM_STORE_LOCAL_CAP);

	if(flags & MAP_PRIVATE) {
		perms &= ~CHERI_PERM_GLOBAL;
	} else if(flags & MAP_SHARED) {

	} else {
		errno = EINVAL;
		return NULL;
	}

	if(prot & PROT_READ) {
		perms |= CHERI_PERM_LOAD;
		if(!(prot & PROT_NO_READ_CAP))
			perms |= CHERI_PERM_LOAD_CAP;
	}
	if(prot & PROT_WRITE) {
		perms |= CHERI_PERM_STORE;
		if(!(prot & PROT_NO_WRITE_CAP)) {
			perms |= CHERI_PERM_STORE_CAP;
			perms |= CHERI_PERM_STORE_LOCAL_CAP;
		}
	}

	if(prot & PROT_EXECUTE) {
		// Our system won't actually support W^X, we will lose one depending on where we derive from
		perms |= CHERI_PERM_EXECUTE;
	}

	int res = _mmap(0, length, perms, flags, &pair);

	if(res == MAP_SUCCESS_INT) {
		if((prot & PROT_EXECUTE) == 0) {
			return pair.data;
		} else {
			return pair.code;
		}
	} else return NULL;
}

int mmap_new(size_t base, size_t length, int cheri_perms, int flags, cap_pair* result) {
	return _mmap(base, length, cheri_perms, flags, result);
}

int munmap(void *addr, size_t length) {
	return MESSAGE_SYNC_SEND_r(memmgt_ref, length, 0, 0, 0, addr, NULL, NULL, NULL, 1);
}

void mmap_set_act(act_kt ref) {
	memmgt_ref = ref;
}
