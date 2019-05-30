/*-
 * Copyright (c) 2016 Alexandre Joannou
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

#ifndef STATCOUNTERS_H
#define STATCOUNTERS_H

/*
 * internals
 */

/* available modules */
#define ICACHE      8   /* CacheCore */
#define DCACHE      9   /* CacheCore */
#define L2CACHE     10  /* CacheCore */
#define MIPSMEM     11  /* MIPSMem   */
#define TAGCACHE    12  /* CacheCore */

/* CacheCore counters */
enum {
    WRITE_HIT     = 0,
    WRITE_MISS    = 1,
    READ_HIT      = 2,
    READ_MISS     = 3,
    PFTCH_HIT     = 4,
    PFTCH_MISS    = 5,
    EVICT         = 6,
    PFTCH_EVICT   = 7,
    SET_TAG_WRITE = 8,
    SET_TAG_READ  = 9
};

/* MIPSMem counters */
enum {
    BYTE_READ   = 0,
    BYTE_WRITE  = 1,
    HWORD_READ  = 2,
    HWORD_WRITE = 3,
    WORD_READ   = 4,
    WORD_WRITE  = 5,
    DWORD_READ  = 6,
    DWORD_WRITE = 7,
    CAP_READ    = 8,
    CAP_WRITE   = 9
};

/* Assembly primitives to access the hardware counters */

/* TODO the itlbmiss/dtlbmiss/cycle/inst counters are not reset with that */
static inline void resetStatCounters (void)
{
    __asm __volatile(".word (0x1F << 26) | (0x0 << 21) | (0x0 << 16) | (0x7 << 11) | (0x0 << 6) | (0x3B)");
}

#include <inttypes.h>

#define DEFINE_GET_STAT_COUNTER(name,X,Y, ...)   \
static inline uint64_t get_##name##_count (void)					\
{										\
	uint64_t ret;								\
	__asm __volatile(".word (0x1f << 26) | (0x0 << 21) | (12 << 16) | ("#X" << 11) | ( "#Y"  << 6) | 0x3b\n" \
	                 "move %0,$12"						\
	                 : "=r" (ret) :: "$12");				\
	return ret;								\
}

/*
 * exported libstatcounters API
 */

/* Map to appropriate RDHWR indices */

#define STAT_ALL_LIST(ITEM, ...)\
	ITEM(cycle,2,0,__VA_ARGS__)\
	ITEM(inst,4,0,__VA_ARGS__)\
	ITEM(itlb_miss,5,0,__VA_ARGS__)\
	ITEM(dtlb_miss,6,0,__VA_ARGS__)\
	ITEM(icache_write_hit,8,0,__VA_ARGS__)\
	ITEM(icache_write_miss,8,1,__VA_ARGS__)\
	ITEM(icache_read_hit,8,2,__VA_ARGS__)\
	ITEM(icache_read_miss,8,3,__VA_ARGS__)\
	ITEM(icache_evict,8,6,__VA_ARGS__)\
	ITEM(dcache_write_hit,9,0,__VA_ARGS__)\
	ITEM(dcache_write_miss,9,1,__VA_ARGS__)\
	ITEM(dcache_read_hit,9,2,__VA_ARGS__)\
	ITEM(dcache_read_miss,9,3,__VA_ARGS__)\
	ITEM(dcache_evict,9,6,__VA_ARGS__)\
	ITEM(dcache_set_tag_write,9,8,__VA_ARGS__)\
	ITEM(dcache_set_tag_read,9,9,__VA_ARGS__)\
	ITEM(l2cache_write_hit,10,0,__VA_ARGS__)\
	ITEM(l2cache_write_miss,10,1,__VA_ARGS__)\
	ITEM(l2cache_read_hit,10,2,__VA_ARGS__)\
	ITEM(l2cache_read_miss,10,3,__VA_ARGS__)\
	ITEM(l2cache_evict,10,6,__VA_ARGS__)\
	ITEM(l2cache_set_tag_write,10,8,__VA_ARGS__)\
	ITEM(l2cache_set_tag_read,10,9,__VA_ARGS__)\
	ITEM(mem_byte_read,11,0,__VA_ARGS__)\
	ITEM(mem_byte_write,11,1,__VA_ARGS__)\
	ITEM(mem_hword_read,11,2,__VA_ARGS__)\
	ITEM(mem_hword_write,11,3,__VA_ARGS__)\
	ITEM(mem_word_read,11,4,__VA_ARGS__)\
	ITEM(mem_word_write,11,5,__VA_ARGS__)\
	ITEM(mem_dword_read,11,6,__VA_ARGS__)\
	ITEM(mem_dword_write,11,7,__VA_ARGS__)\
	ITEM(mem_cap_read,11,8,__VA_ARGS__)\
	ITEM(mem_cap_write,11,9,__VA_ARGS__)\
	ITEM(tagcache_write_hit,12,0,__VA_ARGS__)\
	ITEM(tagcache_write_miss,12,1,__VA_ARGS__)\
	ITEM(tagcache_read_hit,12,2,__VA_ARGS__)\
	ITEM(tagcache_read_miss,12,3,__VA_ARGS__)\
	ITEM(tagcache_evict,12,6,__VA_ARGS__)


STAT_ALL_LIST(DEFINE_GET_STAT_COUNTER)

#endif
