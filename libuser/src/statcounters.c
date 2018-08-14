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

#include <stdio.h>
//#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "statcounters.h"

/*
 * libstatcounter API
 */

DEFINE_GET_STAT_COUNTER(cycle,2,0)
DEFINE_GET_STAT_COUNTER(inst,4,0)
DEFINE_GET_STAT_COUNTER(itlb_miss,5,0)
DEFINE_GET_STAT_COUNTER(dtlb_miss,6,0)
DEFINE_GET_STAT_COUNTER(icache_write_hit,8,0)
DEFINE_GET_STAT_COUNTER(icache_write_miss,8,1)
DEFINE_GET_STAT_COUNTER(icache_read_hit,8,2)
DEFINE_GET_STAT_COUNTER(icache_read_miss,8,3)
DEFINE_GET_STAT_COUNTER(icache_evict,8,6)
DEFINE_GET_STAT_COUNTER(dcache_write_hit,9,0)
DEFINE_GET_STAT_COUNTER(dcache_write_miss,9,1)
DEFINE_GET_STAT_COUNTER(dcache_read_hit,9,2)
DEFINE_GET_STAT_COUNTER(dcache_read_miss,9,3)
DEFINE_GET_STAT_COUNTER(dcache_evict,9,6)
DEFINE_GET_STAT_COUNTER(dcache_set_tag_write,9,8)
DEFINE_GET_STAT_COUNTER(dcache_set_tag_read,9,9)
DEFINE_GET_STAT_COUNTER(l2cache_write_hit,10,0)
DEFINE_GET_STAT_COUNTER(l2cache_write_miss,10,1)
DEFINE_GET_STAT_COUNTER(l2cache_read_hit,10,2)
DEFINE_GET_STAT_COUNTER(l2cache_read_miss,10,3)
DEFINE_GET_STAT_COUNTER(l2cache_evict,10,6)
DEFINE_GET_STAT_COUNTER(l2cache_set_tag_write,10,8)
DEFINE_GET_STAT_COUNTER(l2cache_set_tag_read,10,9)
DEFINE_GET_STAT_COUNTER(mem_byte_read,11,0)
DEFINE_GET_STAT_COUNTER(mem_byte_write,11,1)
DEFINE_GET_STAT_COUNTER(mem_hword_read,11,2)
DEFINE_GET_STAT_COUNTER(mem_hword_write,11,3)
DEFINE_GET_STAT_COUNTER(mem_word_read,11,4)
DEFINE_GET_STAT_COUNTER(mem_word_write,11,5)
DEFINE_GET_STAT_COUNTER(mem_dword_read,11,6)
DEFINE_GET_STAT_COUNTER(mem_dword_write,11,7)
DEFINE_GET_STAT_COUNTER(mem_cap_read,11,8)
DEFINE_GET_STAT_COUNTER(mem_cap_write,11,9)
DEFINE_GET_STAT_COUNTER(tagcache_write_hit,12,0)
DEFINE_GET_STAT_COUNTER(tagcache_write_miss,12,1)
DEFINE_GET_STAT_COUNTER(tagcache_read_hit,12,2)
DEFINE_GET_STAT_COUNTER(tagcache_read_miss,12,3)
DEFINE_GET_STAT_COUNTER(tagcache_evict,12,6)

/* reset the hardware statcounters */
void reset_statcounters (void)
{
    resetStatCounters();
}

/* zero a statcounters_bank */
void zero_statcounters (statcounters_bank_t * const cnt_bank)
{
	if (cnt_bank == NULL) {
		errno = -1;
	} else {
		memset(cnt_bank, 0, sizeof(statcounters_bank_t));
	}
}

/* sample hardware counters in a statcounters_bank */
void sample_statcounters (statcounters_bank_t * const cnt_bank)
{
    if (cnt_bank == NULL)
        errno = -1;
    else
    {
        cnt_bank->icache[WRITE_HIT]         = get_icache_write_hit_count();
        cnt_bank->icache[WRITE_MISS]        = get_icache_write_miss_count();
        cnt_bank->icache[READ_HIT]          = get_icache_read_hit_count();
        cnt_bank->icache[READ_MISS]         = get_icache_read_miss_count();
        cnt_bank->icache[EVICT]             = get_icache_evict_count();
        cnt_bank->dcache[WRITE_HIT]         = get_dcache_write_hit_count();
        cnt_bank->dcache[WRITE_MISS]        = get_dcache_write_miss_count();
        cnt_bank->dcache[READ_HIT]          = get_dcache_read_hit_count();
        cnt_bank->dcache[READ_MISS]         = get_dcache_read_miss_count();
        cnt_bank->dcache[EVICT]             = get_dcache_evict_count();
        cnt_bank->dcache[SET_TAG_WRITE]     = get_dcache_set_tag_write_count();
        cnt_bank->dcache[SET_TAG_READ]      = get_dcache_set_tag_read_count();
        cnt_bank->l2cache[WRITE_HIT]        = get_l2cache_write_hit_count();
        cnt_bank->l2cache[WRITE_MISS]       = get_l2cache_write_miss_count();
        cnt_bank->l2cache[READ_HIT]         = get_l2cache_read_hit_count();
        cnt_bank->l2cache[READ_MISS]        = get_l2cache_read_miss_count();
        cnt_bank->l2cache[EVICT]            = get_l2cache_evict_count();
        cnt_bank->l2cache[SET_TAG_WRITE]    = get_l2cache_set_tag_write_count();
        cnt_bank->l2cache[SET_TAG_READ]     = get_l2cache_set_tag_read_count();
        cnt_bank->tagcache[WRITE_HIT]       = get_tagcache_write_hit_count();
        cnt_bank->tagcache[WRITE_MISS]      = get_tagcache_write_miss_count();
        cnt_bank->tagcache[READ_HIT]        = get_tagcache_read_hit_count();
        cnt_bank->tagcache[READ_MISS]       = get_tagcache_read_miss_count();
        cnt_bank->tagcache[EVICT]           = get_tagcache_evict_count();
        cnt_bank->mipsmem[BYTE_READ]        = get_mem_byte_read_count();
        cnt_bank->mipsmem[BYTE_WRITE]       = get_mem_byte_write_count();
        cnt_bank->mipsmem[HWORD_READ]       = get_mem_hword_read_count();
        cnt_bank->mipsmem[HWORD_WRITE]      = get_mem_hword_write_count();
        cnt_bank->mipsmem[WORD_READ]        = get_mem_word_read_count();
        cnt_bank->mipsmem[WORD_WRITE]       = get_mem_word_write_count();
        cnt_bank->mipsmem[DWORD_READ]       = get_mem_dword_read_count();
        cnt_bank->mipsmem[DWORD_WRITE]      = get_mem_dword_write_count();
        cnt_bank->mipsmem[CAP_READ]         = get_mem_cap_read_count();
        cnt_bank->mipsmem[CAP_WRITE]        = get_mem_cap_write_count();
        cnt_bank->dtlb_miss                 = get_dtlb_miss_count();
        cnt_bank->itlb_miss                 = get_itlb_miss_count();
        cnt_bank->inst                      = get_inst_count();
        cnt_bank->cycle                     = get_cycle_count();
    }
}

/* diff two statcounters_banks into a third one */
void diff_statcounters (
    const statcounters_bank_t * const be,
    const statcounters_bank_t * const bs,
    statcounters_bank_t * const bd)
{
    if (bs == NULL || be == NULL || be == NULL)
        errno = -1;
    else
    {
        bd->itlb_miss    = be->itlb_miss - bs->itlb_miss;
        bd->dtlb_miss    = be->dtlb_miss - bs->dtlb_miss;
        bd->cycle        = be->cycle - bs->cycle;
        bd->inst         = be->inst - bs->inst;
        for (int i = 0; i < MAX_MOD_CNT; i++)
        {
            bd->icache[i]    = be->icache[i] - bs->icache[i];
            bd->dcache[i]    = be->dcache[i] - bs->dcache[i];
            bd->l2cache[i]   = be->l2cache[i] - bs->l2cache[i];
            bd->mipsmem[i]   = be->mipsmem[i] - bs->mipsmem[i];
            bd->tagcache[i]  = be->tagcache[i] - bs->tagcache[i];
        }
    }
}

static int
echo(FILE *f, const char *fmt, ...)
{
	if(f != NULL) {
		panic("not implememted");
	}

	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = vprintf(fmt, ap);
	va_end(ap);

	return (retval);
}

/* dump a statcounters_bank in a file (csv or human readable) */
int dump_statcounters (
	const statcounters_bank_t * const b,
	const char * const fname __unused,
	const char * const fmt)
{
	if ((b == NULL)) {
		return -1;
	}
	#if 0
	if(fname == NULL)) {
		return -1;
	}
	#endif
	bool display_header = true;
	#if 0
	if (access(fname, F_OK) != -1) {
	    display_header = false;
	}
	#endif
	#if 0
	FILE * fp = fopen(fname, "a");
	if(fp == NULL) {
		return -1;
	}
	#else
	FILE * fp = NULL;
	#endif
	if (fmt && (strcmp(fmt,"csv") == 0))
	{
		if (display_header)
		{
			echo(fp, "progname,");
			echo(fp, "cycles,");
			echo(fp, "instructions,");
			echo(fp, "itlb_miss,");
			echo(fp, "dtlb_miss,");
			echo(fp, "icache_write_hit,");
			echo(fp, "icache_write_miss,");
			echo(fp, "icache_read_hit,");
			echo(fp, "icache_read_miss,");
			echo(fp, "icache_evict,");
			echo(fp, "dcache_write_hit,");
			echo(fp, "dcache_write_miss,");
			echo(fp, "dcache_read_hit,");
			echo(fp, "dcache_read_miss,");
			echo(fp, "dcache_evict,");
			echo(fp, "dcache_set_tag_write,");
			echo(fp, "dcache_set_tag_read,");
			echo(fp, "l2cache_write_hit,");
			echo(fp, "l2cache_write_miss,");
			echo(fp, "l2cache_read_hit,");
			echo(fp, "l2cache_read_miss,");
			echo(fp, "l2cache_evict,");
			echo(fp, "l2cache_set_tag_write,");
			echo(fp, "l2cache_set_tag_read,");
			echo(fp, "tagcache_write_hit,");
			echo(fp, "tagcache_write_miss,");
			echo(fp, "tagcache_read_hit,");
			echo(fp, "tagcache_read_miss,");
			echo(fp, "tagcache_evict,");
			echo(fp, "mipsmem_byte_read,");
			echo(fp, "mipsmem_byte_write,");
			echo(fp, "mipsmem_hword_read,");
			echo(fp, "mipsmem_hword_write,");
			echo(fp, "mipsmem_word_read,");
			echo(fp, "mipsmem_word_write,");
			echo(fp, "mipsmem_dword_read,");
			echo(fp, "mipsmem_dword_write,");
			echo(fp, "mipsmem_cap_read,");
			echo(fp, "mipsmem_cap_write");
			echo(fp, "\n");
		}
			echo(fp, "%s,",getprogname());
			echo(fp, "%lu,",b->cycle);
			echo(fp, "%lu,",b->inst);
			echo(fp, "%lu,",b->itlb_miss);
			echo(fp, "%lu,",b->dtlb_miss);
			echo(fp, "%lu,",b->icache[WRITE_HIT]);
			echo(fp, "%lu,",b->icache[WRITE_MISS]);
			echo(fp, "%lu,",b->icache[READ_HIT]);
			echo(fp, "%lu,",b->icache[READ_MISS]);
			echo(fp, "%lu,",b->icache[EVICT]);
			echo(fp, "%lu,",b->dcache[WRITE_HIT]);
			echo(fp, "%lu,",b->dcache[WRITE_MISS]);
			echo(fp, "%lu,",b->dcache[READ_HIT]);
			echo(fp, "%lu,",b->dcache[READ_MISS]);
			echo(fp, "%lu,",b->dcache[EVICT]);
			echo(fp, "%lu,",b->dcache[SET_TAG_WRITE]);
			echo(fp, "%lu,",b->dcache[SET_TAG_READ]);
			echo(fp, "%lu,",b->l2cache[WRITE_HIT]);
			echo(fp, "%lu,",b->l2cache[WRITE_MISS]);
			echo(fp, "%lu,",b->l2cache[READ_HIT]);
			echo(fp, "%lu,",b->l2cache[READ_MISS]);
			echo(fp, "%lu,",b->l2cache[EVICT]);
			echo(fp, "%lu,",b->l2cache[SET_TAG_WRITE]);
			echo(fp, "%lu,",b->l2cache[SET_TAG_READ]);
			echo(fp, "%lu,",b->tagcache[WRITE_HIT]);
			echo(fp, "%lu,",b->tagcache[WRITE_MISS]);
			echo(fp, "%lu,",b->tagcache[READ_HIT]);
			echo(fp, "%lu,",b->tagcache[READ_MISS]);
			echo(fp, "%lu,",b->tagcache[EVICT]);
			echo(fp, "%lu,",b->mipsmem[BYTE_READ]);
			echo(fp, "%lu,",b->mipsmem[BYTE_WRITE]);
			echo(fp, "%lu,",b->mipsmem[HWORD_READ]);
			echo(fp, "%lu,",b->mipsmem[HWORD_WRITE]);
			echo(fp, "%lu,",b->mipsmem[WORD_READ]);
			echo(fp, "%lu,",b->mipsmem[WORD_WRITE]);
			echo(fp, "%lu,",b->mipsmem[DWORD_READ]);
			echo(fp, "%lu,",b->mipsmem[DWORD_WRITE]);
			echo(fp, "%lu,",b->mipsmem[CAP_READ]);
			echo(fp, "%lu" ,b->mipsmem[CAP_WRITE]);
			echo(fp, "\n");
	}
	else
	{
		echo(fp, "===== %s =====\n",getprogname());
		echo(fp, "cycles:               \t%lu\n",b->cycle);
		echo(fp, "instructions:         \t%lu\n",b->inst);
		echo(fp, "itlb_miss:            \t%lu\n",b->itlb_miss);
		echo(fp, "dtlb_miss:            \t%lu\n",b->dtlb_miss);
		echo(fp, "\n");
		echo(fp, "icache_write_hit:     \t%lu\n",b->icache[WRITE_HIT]);
		echo(fp, "icache_write_miss:    \t%lu\n",b->icache[WRITE_MISS]);
		echo(fp, "icache_read_hit:      \t%lu\n",b->icache[READ_HIT]);
		echo(fp, "icache_read_miss:     \t%lu\n",b->icache[READ_MISS]);
		echo(fp, "icache_evict:         \t%lu\n",b->icache[EVICT]);
		echo(fp, "\n");
		echo(fp, "dcache_write_hit:     \t%lu\n",b->dcache[WRITE_HIT]);
		echo(fp, "dcache_write_miss:    \t%lu\n",b->dcache[WRITE_MISS]);
		echo(fp, "dcache_read_hit:      \t%lu\n",b->dcache[READ_HIT]);
		echo(fp, "dcache_read_miss:     \t%lu\n",b->dcache[READ_MISS]);
		echo(fp, "dcache_evict:         \t%lu\n",b->dcache[EVICT]);
		echo(fp, "dcache_set_tag_write: \t%lu\n",b->dcache[SET_TAG_WRITE]);
		echo(fp, "dcache_set_tag_read:  \t%lu\n",b->dcache[SET_TAG_READ]);
		echo(fp, "\n");
		#if 0
		echo(fp, "l2cache_write_hit:    \t%lu\n",b->l2cache[WRITE_HIT]);
		echo(fp, "l2cache_write_miss:   \t%lu\n",b->l2cache[WRITE_MISS]);
		echo(fp, "l2cache_read_hit:     \t%lu\n",b->l2cache[READ_HIT]);
		echo(fp, "l2cache_read_miss:    \t%lu\n",b->l2cache[READ_MISS]);
		echo(fp, "l2cache_evict:        \t%lu\n",b->l2cache[EVICT]);
		echo(fp, "l2cache_set_tag_write:\t%lu\n",b->l2cache[SET_TAG_WRITE]);
		echo(fp, "l2cache_set_tag_read: \t%lu\n",b->l2cache[SET_TAG_READ]);
		echo(fp, "\n");
		#endif
		echo(fp, "tagcache_write_hit:   \t%lu\n",b->tagcache[WRITE_HIT]);
		echo(fp, "tagcache_write_miss:  \t%lu\n",b->tagcache[WRITE_MISS]);
		echo(fp, "tagcache_read_hit:    \t%lu\n",b->tagcache[READ_HIT]);
		echo(fp, "tagcache_read_miss:   \t%lu\n",b->tagcache[READ_MISS]);
		echo(fp, "tagcache_evict:       \t%lu\n",b->tagcache[EVICT]);
		echo(fp, "\n");
		echo(fp, "mem_byte_read:        \t%lu\n",b->mipsmem[BYTE_READ]);
		echo(fp, "mem_byte_write:       \t%lu\n",b->mipsmem[BYTE_WRITE]);
		echo(fp, "mem_hword_read:       \t%lu\n",b->mipsmem[HWORD_READ]);
		echo(fp, "mem_hword_write:      \t%lu\n",b->mipsmem[HWORD_WRITE]);
		echo(fp, "mem_word_read:        \t%lu\n",b->mipsmem[WORD_READ]);
		echo(fp, "mem_word_write:       \t%lu\n",b->mipsmem[WORD_WRITE]);
		echo(fp, "mem_dword_read:       \t%lu\n",b->mipsmem[DWORD_READ]);
		echo(fp, "mem_dword_write:      \t%lu\n",b->mipsmem[DWORD_WRITE]);
		echo(fp, "mem_cap_read:         \t%lu\n",b->mipsmem[CAP_READ]);
		echo(fp, "mem_cap_write:        \t%lu\n",b->mipsmem[CAP_WRITE]);
		echo(fp, "\n");
	}
	return 0;
}

#if 0
/*
 * C constructor / atexit interface
 */

static statcounters_bank_t start_cnt;
static statcounters_bank_t end_cnt;
static statcounters_bank_t diff_cnt;

static void end_sample (void);

__attribute__((constructor))
static void start_sample (void)
{
    atexit(end_sample);
    //printf("resetting stat counters\n");
    //resetStatCounters();
    //printf("initial sampling\n");
    sample_statcounters(&start_cnt);
}

//__attribute__((destructor)) static void end_sample (void)
static void end_sample (void)
{
    //printf("final sampling\n");
    sample_statcounters(&end_cnt); // TODO change the order of sampling to keep cycle sampled early
    diff_statcounters(&end_cnt,&start_cnt,&diff_cnt);
    dump_statcounters(&diff_cnt,getenv("STATCOUNTERS_OUTPUT"),getenv("STATCOUNTERS_FORMAT"));
}
#endif
