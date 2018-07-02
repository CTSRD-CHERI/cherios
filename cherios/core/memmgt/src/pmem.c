/*-
 * Copyright (c) 2017 Lawrence Esswood
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

#include <queue.h>
#include <nano/nanotypes.h>
#include "syscalls.h"
#include "pmem.h"
#include "nano/nanokernel.h"
#include "assert.h"
#include "stdio.h"
#include "mmap.h"
#include "math.h"
#include "object.h"

page_t* book;

void pmem_check_phy_entry(size_t pagen) {
    if(pagen != 0) {
        size_t prv = book[pagen].prev;
        assert_int_ex(book[prv].len + prv, ==, pagen);
    }
    size_t nxt = pagen + book[pagen].len;
    assert_int_ex(nxt, <=, BOOK_END);
    if(nxt != BOOK_END) {
        assert_int_ex(book[nxt].prev, ==, pagen);
    }
}

void pmem_check_book(void) {
    size_t pagen = 0;
    size_t ppagen = (size_t)-1;

    while(pagen < BOOK_END) {
        size_t len = book[pagen].len;
        size_t prv = book[pagen].prev;

        assert_int_ex(len, !=, 0);

        if(ppagen != -1) {
            assert_int_ex(prv, ==, ppagen);
        }

        ppagen = pagen;
        pagen +=len;
    }

    if(pagen != BOOK_END) {
        printf("Book ended at %lx instead of %lx\n", pagen, BOOK_END);
    }
    assert_int_ex(pagen, ==, BOOK_END);
}

static int pmem_try_merge_after(size_t page_n) {
    size_t after = page_n + book[page_n].len;
    if((after != BOOK_END) && book[page_n].status == book[after].status) {
        merge_phy_page_range(page_n);
        pmem_check_phy_entry(page_n);
        assert(book[after].len == 0);
        return 1;
    }
    return 0;
}

size_t pmem_try_merge(size_t page_n) {
    assert(page_n < TOTAL_PHY_PAGES);

    // This can now also fail to to racing the cleaning. We just do our best to merge

    size_t before = book[page_n].prev;
    size_t after = page_n + book[page_n].len;

    if(book[before].status == book[page_n].status) {
        merge_phy_page_range(before);
        if(book[page_n].len == 0) {
            page_n = before;
        }
    }

    if((after != BOOK_END) && book[page_n].status == book[after].status) {
        merge_phy_page_range(page_n);
    }

    return page_n;
}

void pmem_print_book(page_t *book, size_t page_n, size_t times) {
    while(times-- > 0) {
        printf("%p addr: page: %lx. state = %d. len = %lx. prev = %lx\n",
               &book[page_n],
               page_n,
               book[page_n].status,
               book[page_n].len,
               book[page_n].prev);
        page_n = book[page_n].len + page_n;
        if(page_n == BOOK_END) break;
    }

}

void pmem_break_page_to(size_t page_n, size_t len) {
    assert_int_ex(len, >, 0);
    assert_int_ex(len, <, TOTAL_PHY_PAGES);

    if(book[page_n].len == len) return;

    if(book[page_n].len > len && len != 0) {
        split_phy_page_range(page_n, len);
        pmem_check_phy_entry(page_n + len);
    } else {
        printf("len is %lx. Tried to split to %lx\n", book[page_n].len, len);
        assert(0);
    }
}

size_t pmem_get_valid_page_entry(size_t page_n) {
    assert(page_n < TOTAL_PHY_PAGES);

    if(book[page_n].len != 0) return page_n;

    size_t search_index = 0;

    // TODO here is where a skip list comes in handy.
    // TODO we could also scan linearly, but this is effectively the finest grain of the skip list
    // TODO we could also compact the structure here
    while(book[search_index].len + search_index < page_n) {
        search_index = search_index + book[search_index].len;
    }

    // We had one very large page we need to break. Otherwise we would have returned it straight away
    size_t diff = page_n - search_index;
    pmem_break_page_to(search_index, diff);

    return page_n;
}

static size_t blocked;

void block_finding_page(size_t pagen) {
    blocked = pagen;
}

void unblock_finding_page(void) {
    blocked = 0;
}

size_t pmem_find_page_type(size_t required_len, e_page_status required_type) {
    size_t search_index = 0;

    while((search_index != BOOK_END) && (search_index == blocked || book[search_index].len < required_len || book[search_index].status != required_type)) {
        search_index = search_index + book[search_index].len;
    }

    if(search_index == BOOK_END) return BOOK_END;

    if(book[search_index].len != required_len) {
        pmem_break_page_to(search_index, required_len);
    }

    return search_index;
}

size_t pmem_get_free_page() {
    return pmem_find_page_type(1, page_unused);
}

void full_dump(void) {
    pmem_print_book(book, 0, -1);
    mmap_dump();
}

void __get_physical_capability(size_t base, size_t length, int IO, int cached, mop_t mop_sealed, cap_pair* result) {
    mop_internal_t* mop = unseal_mop(mop_sealed);

    size_t page_n = base >> PHY_PAGE_SIZE_BITS;

    size_t page_len = align_up_to(length + base - (page_n << PHY_PAGE_SIZE_BITS), PHY_PAGE_SIZE) >> PHY_PAGE_SIZE_BITS;

    if(base == 0) {
        page_n = pmem_find_page_type(page_len, page_unused);
        if(page_n == BOOK_END) {
            goto er;
        }
    } else {
        page_n = pmem_get_valid_page_entry(page_n);
        if(book[page_n].len < page_len) {
            while(pmem_try_merge_after(page_n)) {
                if(book[page_n].len >= page_len) break;
            }
            if(book[page_n].len < page_len) {
                goto er;
            }
        }

        if(book[page_n].len > page_len) {
            pmem_break_page_to(page_n, page_len);
        }
    }

    cap_pair pair;

    get_phy_page(page_n, cached, page_len, &pair, IO);

    size_t offset = base - (page_n << PHY_PAGE_SIZE_BITS);
    if(base != 0 && offset != 0) {
        pair.data = cheri_incoffset(pair.data, offset);
        pair.code = cheri_incoffset(pair.code, offset);
    }

    result->data = cheri_setbounds(pair.data, length);
    result->code = cheri_setbounds(pair.code, length);

    return;
    er:
    result->data = result->code = NULL;
    return;
}

void clean_loop(void) {
    size_t page_n = 0;
    syscall_change_priority(act_self_ctrl, PRIO_IDLE); // Only do this when idle.
    sleep(0);
    while(1) {
        // Keep walking through the physical pages, if a dirty one is found, clean it

        if(page_n == BOOK_END || book[page_n].len == 0) {
            page_n = 0;
        }

        if(book[page_n].status == page_dirty) {
            // We don't try merge afterwards. This would interfere with the main thread.
            // Instead these are merged by subsequent searches
            zero_page_range(page_n);
        }

        page_n +=book[page_n].len;
    }
}
