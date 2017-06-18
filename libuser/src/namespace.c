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
#include "object.h"
#include "assert.h"

void * namespace_ref = NULL;
void * namespace_id  = NULL;

void namespace_init(void *ns_ref, void *ns_id) {
	namespace_ref = ns_ref;
	namespace_id  = ns_id;
}

int namespace_register(int nb, void *ref, void *id) {
	return ccall_4(namespace_ref, namespace_id, 0, nb, (register_t)ref, (register_t)id, 0);
}

int namespace_dcall_register(int nb, void *entry, void *base) {
	return ccall_4(namespace_ref, namespace_id, 3, nb, (register_t)entry, (register_t)base, 0);
}

void * namespace_get_ref(int nb) {
	return (void *)ccall_4(namespace_ref, namespace_id, 1, nb, 0, 0, 0);
}

void * namespace_get_id(int nb) {
	return (void *)ccall_4(namespace_ref, namespace_id, 2, nb, 0, 0, 0);
}

void * namespace_get_entry(int nb) {
	return (void *)ccall_4(namespace_ref, namespace_id, 4, nb, 0, 0, 0);
}

void * namespace_get_base(int nb) {
	return (void *)ccall_4(namespace_ref, namespace_id, 5, nb, 0, 0, 0);
}
