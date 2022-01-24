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

#include <math.h>
#include "elf.h"
#include "stddef.h"
#include "sys/mman.h"
#include "object.h"
#include "namespace.h"
#include "stdio.h"
#include "assert.h"
#include "errno.h"

act_kt memmgt_ref = NULL;
// FIXME: Probably should not circumvent the main mmap activation.
// FIXME: Doing this for now to get things to work.
act_kt commit_ref = NULL;
LW_THR mop_t own_mop = NULL;

act_kt try_init_memmgt_ref(void) {
    if(memmgt_ref == NULL) {
        memmgt_ref = namespace_get_ref(namespace_num_memmgt);
    }
    return memmgt_ref;
}

void commit_vmem(__unused act_kt activation, __unused size_t addr) {
    assert(0);
}

MESSAGE_WRAP_ID_ASSERT(size_t, mem_commit_range, (size_t, addr, size_t, pages, mem_request_flags, flags), commit_ref, 12, namespace_num_memmgt_commit)

void *mmap(__unused void *addr, size_t length, int prot, int flags, __unused int fd, __unused off_t offset) {
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
	res_t res = mem_request(0, req_length, NONE, own_mop).val;

	if(res != NULL) {
		rescap_take(res, &pair);
		capability result;
		if((prot & PROT_EXECUTE) == 0) {
			result = pair.data;
		} else {
			result = pair.code;
		}
		return cheri_andperm(result, perms);
	} else return NULL;
}

int munmap(void *addr, size_t length) {
    return mem_release((size_t)addr, length, 1, own_mop);
}

res_t mmap_based_capmalloc(size_t s, Elf_Env* env) {
	precision_rounded_length pr = round_cheri_length(s);
	ERROR_T(res_t) result = mem_request(0, pr.length + pr.mask, NONE, env->handle);

	if(!IS_VALID(result)) return NULL;

	res_t res = reservation_precision_align(result.val, pr.length, pr.mask);

	return res;
}

cap_pair mmap_based_alloc(size_t s, Elf_Env* env) {
    assert(env != NULL);
    assert(env->handle != NULL);
	cap_pair p;

    precision_rounded_length pr;

    pr.mask = 0;

    pr = round_cheri_length(s);

	ERROR_T(res_t) res = mem_request(0, pr.length + pr.mask, NONE, env->handle);

	if(!IS_VALID(res))  {
		return NULL_PAIR;
	}

	res_t reser = reservation_precision_align(res.val, pr.length, pr.mask);

	rescap_take(reser, &p);
	return p;
}

int mmap_based_free(capability c, Elf_Env* env) {
	return mem_release(cheri_getbase(c), cheri_getlen(c), 1, env->handle);
}

ERROR_T(res_t) mem_request(size_t base, size_t length, mem_request_flags flags, mop_t mop) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	void* out = NULL;
	return MAKE_VALID(res_t, message_send_c(MARSHALL_ARGUMENTS(mop, out, base, length, flags), memmgt, SYNC_CALL, 0));
}

ERROR_T(res_t) mem_request_phy_out(size_t base, size_t length, mem_request_flags flags, mop_t mop, size_t* phy_out) {
    act_kt memmgt = try_init_memmgt_ref();
    assert(memmgt != NULL);
    assert(flags & (COMMIT_DMA | COMMIT_NOW));
    _unsafe size_t out = (size_t)(-1);
    ARG_ASSERTS(mop, &out, base, length, flags)
    ERROR_T(res_t) res = MAKE_VALID(res_t, message_send_c(MARSHALL_ARGUMENTS(mop, &out, base, length, flags), memmgt, SYNC_CALL, 0));
    *phy_out = out;
    return res;
}

MESSAGE_WRAP_ID_ASSERT(int, mem_claim, (size_t, base, size_t, length, size_t, times, mop_t, mop), memmgt_ref, 5, namespace_num_memmgt)
MESSAGE_WRAP_ID_ASSERT(int, mem_claim_mode, (size_t, base, size_t, length, size_t, times, mop_t, mop, ccall_selector_t, mode), memmgt_ref, 5, namespace_num_memmgt)
MESSAGE_WRAP_ID_ASSERT(int, mem_release, (size_t, base, size_t, length, size_t, times, mop_t, mop), memmgt_ref, 1, namespace_num_memmgt)
MESSAGE_WRAP_ID_ASSERT(int, mem_release_mode, (size_t, base, size_t, length, size_t, times, mop_t, mop, ccall_selector_t, mode), memmgt_ref, 1, namespace_num_memmgt)
MESSAGE_WRAP_ID_ASSERT_ERRT(mop_t, mem_makemop_debug, (res_t, space, mop_t, auth_mop, const char*, debug_id), memmgt_ref, 7, namespace_num_memmgt)

ERROR_T(mop_t) mem_makemop(res_t space, mop_t auth_mop) {
	return mem_makemop_debug(space, auth_mop, NULL);
}

MESSAGE_WRAP_ID_ASSERT(int, mem_reclaim_mop, (mop_t, mop_sealed), memmgt_ref, 9, namespace_num_memmgt)
MESSAGE_WRAP_ID_ASSERT(mop_t, init_mop, (capability, mop_sealing_cap), memmgt_ref, 6, namespace_num_memmgt)
MESSAGE_WRAP_ID_ASSERT(void, get_physical_capability, (mop_t, mop, cap_pair*, result, size_t, base, size_t, length, int, IO, int, cached), memmgt_ref, 8, namespace_num_memmgt)

void mmap_set_act(act_kt ref) {
	memmgt_ref = ref;
}

void mmap_set_mop(mop_t mop) {
	own_mop = mop;
}

void mdump(void) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	message_send(0,0,0,0,NULL,NULL,NULL,NULL, memmgt, SYNC_CALL, 3);
}

struct revoke_tracking* get_tracking(void) {
	act_kt memmgt = try_init_memmgt_ref();
	assert(memmgt != NULL);
	return (struct revoke_tracking*)message_send_c(0,0,0,0,NULL,NULL,NULL,NULL, memmgt, SYNC_CALL, 14);
}
