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
int __mmap(size_t base, size_t length, int cheri_perms, int flags, cap_pair* result) {
    /* We -might- reserve one of these for a can-free perm. I would prefer to use a sealed cap.
     * Types are numerous. Permissions are not. */

    result->data = NULL;
    result->code = NULL;


	if(!(flags & MAP_ANONYMOUS)) {
		errno = EINVAL;
		goto fail;
	}
	if((flags & MAP_PRIVATE) && (flags & MAP_SHARED)) {
		errno = EINVAL;
		goto fail;
	}

    cap_pair pair;
    /* Because people ask for silly lengths, round up to a cap */
    length += ((CHERICAP_SIZE - (length & (CHERICAP_SIZE-1))) & (CHERICAP_SIZE-1));

    if(flags & MAP_PHY) {
        if(base == 0) {
            size_t npages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
            size_t page_index = find_page_type(npages, page_unused);
            get_phy_page(page_index, (cheri_perms & MAP_CACHED) != 0, npages, &pair);
        } else {
            size_t npages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
            size_t page_index = base / PAGE_SIZE;
            size_t page_offset = (base & (PAGE_SIZE-1));

            /* For overlapping reasons */
            if(((npages * PAGE_SIZE) - page_offset) < length) npages++;

            page_index = get_valid_page_entry(page_index);
            size_t of_length = book[page_index].len;

            if((of_length < npages) || (book[page_index].status != page_unused)) {
                printf(KRED"A physical range was requested with a range that overlaps a taken region."
                               "Requested pages %lx. Region of size %lx. State: %d \n"KRST,
                       npages, of_length, book[page_index].status);
                print_book(book, 0, 1000);
                goto fail;
            }

            if(of_length > npages) {
                break_page_to(page_index, npages);
            }

            get_phy_page(page_index, (flags & MAP_CACHED) != 0, npages, &pair);

            if(pair.data != NULL) pair.data = cheri_setoffset(pair.data, page_offset);
            if(pair.code != NULL) pair.code = cheri_setoffset(pair.code, page_offset);
        }

        if(pair.data != NULL) pair.data = cheri_setbounds(pair.data, length);
        if(pair.code != NULL) pair.code = cheri_setbounds(pair.code, length);

    } else {
        if(base == 0) {
            /* TODO we probably need a reference here */
            memgt_take_reservation(length, NULL, &pair);
        } else {
            printf("I have not finished virtual memory yet - only the first few pages are allocated =(\n");
            goto fail;
        }
    }

 ok:

    result->data = cheri_andperm(pair.data, cheri_perms);
    if(cheri_perms & CHERI_PERM_EXECUTE) {
        result->code = cheri_andperm(pair.code, cheri_perms);
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