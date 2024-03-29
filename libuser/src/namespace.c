/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "stddef.h"
#include "object.h"
#include "cheric.h"
#include "assert.h"
#include "stdio.h"
#include "capmalloc.h"

act_kt namespace_ref = NULL;

int namespace_rdy(void) {
	return namespace_ref != NULL;
}

void namespace_init(act_kt ns_ref) {
	namespace_ref = ns_ref;
}

MESSAGE_WRAP(int, namespace_register, (int, nb, act_kt, ref), namespace_ref, 0)
MESSAGE_WRAP(act_kt, namespace_get_ref, (int, nb), namespace_ref, 1)
MESSAGE_WRAP(int, namespace_register_found_id, (cert_t, cert), namespace_ref, 4)
MESSAGE_WRAP(found_id_t*, namespace_get_found_id, (int, nb), namespace_ref, 3)
MESSAGE_WRAP_DEF(int, namespace_get_num_services, (void), namespace_ref, 2, -1)
MESSAGE_WRAP_DEF(int, namespace_register_name, (const char*, name, act_kt, ref), namespace_ref, 5, -1)
MESSAGE_WRAP_DEF(act_kt, namespace_get_ref_by_name, (const char*, name), namespace_ref, 6, NULL)
