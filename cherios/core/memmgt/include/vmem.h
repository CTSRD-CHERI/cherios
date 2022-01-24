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

#ifndef CHERIOS_VMEM_H
#define CHERIOS_VMEM_H

#include "sys/mman.h"
#include "nano/nanokernel.h"
#include "assert.h"

mop_t	mem_minit(capability mop_sealing_cap);

void vmem_commit_vmem(act_kt activation, char* name, size_t addr);
size_t __vmem_commit_vmem_range(size_t addr, size_t pages, mem_request_flags flags);

/* Allocates a page table, but finds a physical page for you */
ptable_t vmem_create_table(ptable_t parent, register_t index, int level);

/* Creates a virt->phy mapping but chooses a physical page for you */
int vmem_create_mapping(ptable_t L2_table, register_t index, register_t flags);

/* Will free n pages staring at vaddr_start, also freeing tables as required */
void vmem_free_range(size_t vaddr_start, size_t pages);

void vmem_free_single(size_t vaddr);

typedef void vmem_visit_func(capability arg, ptable_t table, readable_table_t* RO, size_t index, size_t rep_pages);

void vmem_visit_range(size_t page_start, size_t pages, vmem_visit_func*, capability arg);

size_t virtual_to_physical(size_t vaddr);

extern __thread int worker_id;
#endif //CHERIOS_VMEM_H
