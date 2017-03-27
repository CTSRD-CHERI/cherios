/*-
 * Copyright (c) 2011 Robert N. M. Watson
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

#ifndef _EXPORT_CHERIOS_KLIB_H_
#define	_EXPORT_CHERIOS_KLIB_H_

#include "cheric.h"
#include "queue.h"

// FIXME we need to really think about the types of IDs and REFs

/* The type of object activation references */
static const uint64_t act_ref_type = 0x42002;
/* The type of object identifier references */
static const uint64_t act_id_type = act_ref_type;
/* The type of object activation identifier control references */
static const uint64_t act_ctrl_ref_type = 0x42001;
/* The type of the synchronous sequence reply token */
static const uint64_t act_sync_type = 0x42000;
/* The type of object activation response references */
static const uint64_t act_sync_ref_type = 0x42003;

void	kernel_ccall(void);
void	kernel_creturn(void);

#ifndef _CHERIOS_KLIB_H_
#define act_t	void
#define act_control_t	void
#define status_e	unsigned
#endif

act_control_t *	act_register(const reg_frame_t * frame, queue_t * queue, const char * name, register_t a0, status_e create_in_status);
act_t *	act_get_sealed_ref_from_ctrl(act_control_t * ctrl);

#ifndef _CHERIOS_KLIB_H_
#undef act_t
#undef act_control_t
#endif

#endif /* _EXPORT_CHERIOS_KLIB_H_ */
