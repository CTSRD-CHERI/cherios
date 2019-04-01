/*-
 * Copyright (c) 2019 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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
#ifndef CHERIOS_SOCKETS_H
#define CHERIOS_SOCKETS_H

#include "socket_common.h"

PLT_ty(lib_socket_if_t, SOCKET_LIB_IF_LIST)
PLT_define(SOCKET_LIB_IF_LIST)

// A sealed type to be passed outside this library
enum {
    invalid_guard_type = 0,
    requester_guard_type = 1,
    fulfiller_guard_type = 2,
} lib_socket_types;

#define UNSEAL_CHECK_REQUESTER(R)     ({ \
    uni_dir_socket_requester* _requester_tmp = cheri_unseal(R, get_cds()); \
    (_requester_tmp->fulfiller_component.guard.guard == MAKE_USER_GUARD_TYPE(requester_guard_type)) ? _requester_tmp : NULL; \
    })

#define UNSEAL_CHECK_FULFILLER(F)     ({ \
    uni_dir_socket_requester* _requester_tmp = cheri_unseal(F, get_cds()); \
    (_requester_tmp->fulfiller_component.guard.guard == MAKE_USER_GUARD_TYPE(fulfiller_guard_type)) ? _requester_tmp : NULL;\
    })

#endif //CHERIOS_SOCKETS_H
