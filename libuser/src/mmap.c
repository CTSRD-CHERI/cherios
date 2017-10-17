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

#include "elf.h"
#include "mips.h"
#include "sys/mman.h"
#include "object.h"
#include "namespace.h"
#include "stdio.h"
#include "assert.h"

act_kt memmgt_ref = NULL;
mop_t own_mop = NULL;

act_kt try_init_memmgt_ref(void) {
    if(memmgt_ref == NULL) {
        memmgt_ref = namespace_get_ref(namespace_num_memmgt);
    }
    return memmgt_ref;
}

void commit_vmem(act_kt activation, size_t addr) {
	if(memmgt_ref == NULL) {
		memmgt_ref = namespace_get_ref(namespace_num_memmgt);
		assert(memmgt_ref != NULL);
	}
	message_send(addr, 0, 0, 0, activation, NULL, NULL, NULL, memmgt_ref, SEND_SWITCH, 2);
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

	size_t req_length = align_up_to(length, RES_META_SIZE);
	res_t res = mem_request(0, req_length, NONE, own_mop);

	if(res != NULL) {
		rescap_take(res, &pair);
		if((prot & PROT_EXECUTE) == 0) {
			return pair.data;
		} else {
			return pair.code;
		}
	} else return NULL;
}

int munmap(void *addr, size_t length) {
    mem_release((size_t)addr, length, 1, own_mop);
}

cap_pair mmap_based_alloc(size_t s, Elf_Env* env) {
    assert(env != NULL);
    assert(env->handle != NULL);
	cap_pair p;
	res_t res = mem_request(0, s, NONE, env->handle);
	if(cheri_gettag(res) == 0)  {
		printf("mmap based alloc failed %ld\n", cap_to_gen(res));
		return NULL_PAIR;
	}
	rescap_take(res, &p);
	return p;
}

void mmap_based_free(capability c, Elf_Env* env) {
	return mem_release(cheri_getbase(c), cheri_getlen(c), 1, env->handle);
}

res_t mem_request(size_t base, size_t length, mem_request_flags flags, mop_t mop) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	return message_send_c(base, length, flags, 0, mop, NULL, NULL, NULL, memmgt, SYNC_CALL, 0);
}

int mem_claim(size_t base, size_t length, size_t times, mop_t mop) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	return (int)message_send(base, length, times, 0, mop, NULL, NULL, NULL, memmgt, SYNC_CALL, 5);
}

int mem_release(size_t base, size_t length, size_t times, mop_t mop) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	return (int)message_send(base, length, times, 0, mop, NULL, NULL, NULL, memmgt, SYNC_CALL, 1);
}

mop_t mem_makemop(res_t space, mop_t auth_mop) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	return message_send_c(0, 0, 0, 0, space, auth_mop, NULL, NULL, memmgt, SYNC_CALL, 7);
}

int mem_reclaim_mop(mop_t mop_sealed) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	return (int)message_send(0, 0, 0, 0, mop_sealed, NULL, NULL, NULL, memmgt, SYNC_CALL, 9);
}
mop_t init_mop(capability mop_sealing_cap) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	return message_send_c(0, 0, 0, 0, mop_sealing_cap, NULL, NULL, NULL, memmgt, SYNC_CALL, 6);
}

void get_physical_capability(size_t base, size_t length, int IO, int cached, mop_t mop, cap_pair* result) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	message_send(base, length, (register_t )IO, (register_t)cached, mop, result, NULL, NULL, memmgt, SYNC_CALL, 8);
}

void mmap_set_act(act_kt ref) {
	memmgt_ref = ref;
}

void mmap_set_mop(mop_t mop) {
	own_mop = mop;
}

void mdump(void) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	message_send(0,0,0,0,NULL,NULL,NULL,NULL, memmgt_ref, SYNC_CALL, 3);
}

size_t mvirtual_to_physical(size_t vaddr) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
    return message_send(vaddr,0,0,0,NULL,NULL,NULL,NULL, memmgt_ref, SYNC_CALL, 4);
}