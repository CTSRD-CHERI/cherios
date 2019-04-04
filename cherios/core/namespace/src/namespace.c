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

#include "lib.h"
#include "sys/mman.h"
#include "syscalls.h"
#include "object.h"
#include "string.h"
#include "namespace.h"
#include "stdio.h"

typedef struct {
	void * act_reference;
} bind_t;

#define BIND_LEN 0x80
bind_t bind[BIND_LEN];
found_id_t* ids[BIND_LEN];
int count;

void ns_init(void) {
	/* We need to bootstrap the namespace refs ourselves using our
	   ctrl cap, since the generic libuser was provided NULL
	   caps. */

	extern act_kt namespace_ref;
	namespace_ref = act_self_ref;

	bzero(bind, sizeof(bind));
	count = 0;
}

static int validate_idx(int nb) {
	if(nb <  0       ) { return -1; }
	if(nb >= BIND_LEN) { return -1; }
	return 0;
}

static int validate_act_caps(void * act_reference) {
	if(cheri_gettag(act_reference) == 0) { return -2; }
	if(cheri_getsealed(act_reference) == 0) { return -3; }
	return 0;
}

/* Get reference for service 'n' */
void * ns_get_reference(int nb) {
	if(validate_idx(nb) != 0) {
		return NULL;
	}
	/* If service not in use, will already return NULL */
	return bind[nb].act_reference;
}

/* Register a module a service 'nb' */
static int ns_register_core(int nb, void * act_reference) {
	if(bind[nb].act_reference != NULL) {
		return -4;
	}

	bind[nb].act_reference  = act_reference;

	/* Use the globally accepted numbers rather than the order
	 */
	if (nb == namespace_num_memmgt) {
		mmap_set_act(act_reference);
		printf("%s: #%d (mem-mgr) registered at port %d\n", __func__, count, nb);
	} else {
		printf("%s: #%d registered at port %d\n", __func__, count, nb);
	}

	count++;
	return 0;
}

int ns_register(int nb, void * act_reference) {

	int ret = validate_idx(nb);
	if(ret != 0) return ret;
	if((ret = validate_act_caps(act_reference)) != 0) return  ret;

	return ns_register_core(nb, act_reference);
}

int ns_get_num_services(void) {
	return count;
}

int ns_register_found_id(cert_t cert) {
    _safe cap_pair pair;
    found_id_t* id = rescap_check_cert(cert, &pair);
    int nb = (int)pair.code;

    if(!id || validate_idx(nb)) return -1;

    ids[nb] = id;

    return 0;
}

found_id_t* ns_get_found_id(int nb) {
    return validate_idx(nb) ? NULL : ids[nb];
}