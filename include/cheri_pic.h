/*-
 * Copyright (c) 2018 Lawrence Esswood
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
#ifndef CHERIOS_CHERI_PIC_H
#define CHERIOS_CHERI_PIC_H

#define CHERI_PIC_SOFT          64
#define CHERI_PIC_SOFT_LENGTH   64
#define CHERI_PIC_HARD          0
#define CHERI_PIC_HARD_LENGTH   64

#define PIC_CONFIG_SZ           0x4000
#define PIC_CONFIG_SZ_LOG_2     (14)
#define PIC_CONFIG_START        0x7f804000

#define PIC_CONFIG_BASE(C)     (PIC_CONFIG_START + (PIC_CONFIG_SZ * (C)))
#define PIC_CONFIG_X(C, X)     (PIC_CONFIG_BASE((C)) + (8 * X))         // access via word or double word
#define PIC_IP_READ_BASE(C)    (PIC_CONFIG_BASE(C) + (8 * 1024))
#define PIC_IP_SET_BASE(C)     (PIC_IP_READ_BASE(C) + 128)              // 1 bit per entry
#define PIC_IP_CLEAR_BASE(C)   (PIC_IP_READ_BASE(C) + 256)              // 1 bit per entry

#define PIC_CONFIG_OFFSET_E     31 // Enable/disable
#define PIC_CONFIG_OFFSET_TID   8  // Thread ID (why do we have different config registers...?)
#define PIC_CONFIG_SIZE_TID     8
#define PIC_CONFIG_OFFSET_IRQ   0
#define PIC_CONFIG_SIZE_IRQ     3 // MIPS external 0-4 map to IP2-6. 5 is or'd with timer. 6/7 unsupported


#endif //CHERIOS_CHERI_PIC_H
