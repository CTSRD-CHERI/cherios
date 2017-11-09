/*-
 * Copyright (c) 2017 Lawrence Esswood
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

#include "tman.h"
#include "sys/types.h"
#include "namespace.h"
#include "msg.h"

act_kt tman_ref = NULL;

static act_kt try_init_tman_ref(void) {
    if(tman_ref == NULL) {
        tman_ref = namespace_get_ref(namespace_num_tman);
    }
    return tman_ref;
}

top_t type_get_first_top(void) {
    return (top_t)message_send_c(0, 0, 0, 0, NULL, NULL, NULL, NULL, tman_ref, SYNC_CALL, 0);
}

ERROR_T(top_t) type_new_top(top_t parent) {
    return (ERROR_T(top_t))message_send_c(0, 0, 0, 0, parent, NULL, NULL, NULL, tman_ref, SYNC_CALL, 1);
}

er_t type_destroy_top(top_t top) {
    return (er_t)message_send_c(0, 0, 0, 0, top, NULL, NULL, NULL, tman_ref, SYNC_CALL, 2);
}

ERROR_T(tres_t) type_get_new(top_t top) {
    return (ERROR_T(tres_t))message_send_c(0, 0, 0, 0, top, NULL, NULL, NULL, tman_ref, SYNC_CALL, 3);
}

ERROR_T(tres_t) type_get_new_exact(top_t top, stype type) {
    return (ERROR_T(tres_t))message_send_c(type, 0, 0, 0, top, NULL, NULL, NULL, tman_ref, SYNC_CALL, 4);
}

er_t type_return_type(top_t top, stype type) {
    return (er_t)message_send_c(type, 0, 0, 0, top, NULL, NULL, NULL, tman_ref, SYNC_CALL, 5);
}