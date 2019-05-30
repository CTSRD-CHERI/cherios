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
#ifndef CHERIOS_HOSTCONFIG_H
#define CHERIOS_HOSTCONFIG_H

#define CHERIOS_NET_MASK    "255.255.255.0"
#define CHERIOS_GATEWAY     "128.232.18.1"
#define CHERIOS_DNS         "128.232.1.1"

// #define LOCAL

#ifdef HARDWARE_qemu

    #define CHERIOS_IP          "128.232.18.56"
    #define CHERIOS_MAC         {0x00,0x16,0x3E,0xE8,0x12,0x38}
    #define CHERIOS_HOST        "cherios"

#else

    #define CHERIOS_MAC         {0xba,0xdb,0xab,0xe5,0xca,0xfe}

    #ifdef LOCAL
        #define CHERIOS_IP          "10.0.0.245"
        #define CHERIOS_HOST        "cherios-fpga-local"
    #else
        #define CHERIOS_IP          "128.232.18.245"
        #define CHERIOS_HOST        "cherios-fpga"
    #endif

#endif

#endif //CHERIOS_HOSTCONFIG_H
