/*-
 * Copyright (c) 2016 Hadrien Barral
 * Copyright (c) 2016 Lawrence Esswood
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

#include "lib.h"
#include "sys/mman.h"
#include "types.h"
#include "utils.h"
#include "vmem.h"

/* fd and offset are currently unused and discarded in userspace */
int __mmap(void *addr, size_t length, int prot, int flags, cap_pair* result) {
	int perms = CHERI_PERM_SOFT_1; /* can-free perm */
    result->data = NULL;
    result->code = NULL;

	if(addr != NULL)
		panic("mmap: addr must be NULL");

	if(!(flags & MAP_ANONYMOUS)) {
		errno = EINVAL;
		goto fail;
	}
	if((flags & MAP_PRIVATE) && (flags & MAP_SHARED)) {
		errno = EINVAL;
		goto fail;
	}

	if(flags & MAP_PRIVATE) {
		perms |= CHERI_PERM_STORE_LOCAL_CAP;
	} else if(flags & MAP_SHARED) {
		perms |= CHERI_PERM_GLOBAL;
	} else {
		errno = EINVAL;
		goto fail;
	}

	if(prot & PROT_READ) {
		perms |= CHERI_PERM_LOAD;
		if(!(prot & PROT_NO_READ_CAP))
			perms |= CHERI_PERM_LOAD_CAP;
	}
	if(prot & PROT_WRITE) {
		perms |= CHERI_PERM_STORE;
		if(!(prot & PROT_NO_WRITE_CAP))
			perms |= CHERI_PERM_STORE_CAP;
	}

	if(prot & PROT_EXECUTE) {
        // Our system won't actually support W^X, we will lose one depending on where we derive from
		perms |= CHERI_PERM_EXECUTE;
	}

    /* TODO we probably need a reference here */
    /* Because people ask for silly lengths, round up to a cap */
    length += ((CHERICAP_SIZE - (length & (CHERICAP_SIZE-1))) & (CHERICAP_SIZE-1));

    cap_pair pair;
    memgt_take_reservation(length, NULL, &pair);
 ok:

    result->data = cheri_andperm(pair.data, perms);

    if(prot & PROT_EXECUTE) {
        result->code = cheri_andperm(pair.code, perms);
        assert((cheri_getperm(result->code) & CHERI_PERM_EXECUTE) != 0);
    }

	return 0;

 fail:
	printf(KRED "mmap fail %lx\n", length);
	return MAP_FAILED_INT;
}


int __munmap(void *addr, size_t length) {
	if(!(cheri_getperm(addr) & CHERI_PERM_SOFT_1)) {
		errno = EINVAL;
		printf(KRED"BAD MUNMAP\n");
		return -1;
	}

	// TODO
	return 0;
}

void mfree(void *addr) {
	// TODO
	return;
}