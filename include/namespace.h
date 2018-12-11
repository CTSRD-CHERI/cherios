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

#ifndef CHERIOS_NAMESPACE_H
#define CHERIOS_NAMESPACE_H

#include "types.h"

int     namespace_rdy(void);
void	namespace_init(act_kt ns_ref);
int	namespace_register(int nb, act_kt ref);
act_kt	namespace_get_ref(int nb);
int	namespace_get_num_services(void);

extern act_kt namespace_ref;

// TODO this is not a good way to handle names, we probably want string ids, or a string to integer id
static const int namespace_num_kernel = 0;
static const int namespace_num_init = 1;
static const int namespace_num_namespace = 2;
static const int namespace_num_proc_manager = 3;
static const int namespace_num_memmgt = 4;
static const int namespace_num_tman = 5;

static const int namespace_num_uart = 0x40;
static const int namespace_num_zlib = 0x41;
static const int namespace_num_sockets = 0x42;
static const int namespace_num_virtio = 0x43;
static const int namespace_num_fs = 0x44;
static const int namespace_num_tcp = 0x45;
static const int namespace_num_blockcache = 0x46;

static const int namespace_num_event_service = 0x61;
static const int namespace_num_dedup_service = 0x62;

static const int namespace_num_webserver = 0x70;
#endif