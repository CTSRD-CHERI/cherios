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

#ifndef CHERIOS_TMAN_H
#define CHERIOS_TMAN_H

#include "cheric.h"
#include "nano/nanotypes.h"
#include "sys/types.h"

#define TOP_SEALING_TYPE 0x999

#define USER_TYPES_START 0x1000
#define USER_TYPES_END   0x2000
#define USER_TYPES_LEN   (USER_TYPES_END - USER_TYPES_START)
typedef capability top_t;


DEC_ERROR_T(top_t);
DEC_ERROR_T(tres_t);

act_kt try_init_tman_ref(void);
top_t type_get_first_top(void);
ERROR_T(top_t) type_new_top(top_t parent);
er_t type_destroy_top(top_t top);
ERROR_T(tres_t) type_get_new(top_t top);
ERROR_T(tres_t) type_get_new_exact(top_t top, stype type);
er_t type_return_type(top_t top, stype type);

#define TYPE_OK                 (0)

#define TYPE_ER_INVALID_TOP     (-1)
#define TYPE_ER_OUT_OF_TYPES    (-2)
#define TYPE_ER_TYPE_USED       (-3)
#define TYPE_ER_OUT_OF_RANGE    (-4)
#define TYPE_ER_DOES_NOT_OWN    (-5)

#endif //CHERIOS_TMAN_H
