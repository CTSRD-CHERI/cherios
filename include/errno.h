/*-
 * Copyright (c) 2020 Lawrence Esswood
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

// Compatibility layer for POSIX errno.
// CheriOS tends to return errors as return parameters and (currently) has no central list.
// If there is a POSIX equivalent we map it, otherwise we make up a new error name.

#ifndef CHERIOS_ERRNO_H
#define CHERIOS_ERRNO_H

#include "ff.h"

// The FR errors are exactly aligned so we can cast
// The CheriOS socket errors (which start at -1) come next and can just be shifted
// Lastly, come errors that have no CheriOS equivalent

#define ER_LIST(ITEM)               \
    ITEM(NO_ER)                     \
    ITEM(EFR_DISK_ERR)              \
    ITEM(EFR_INT_ERR)               \
    ITEM(EFR_NOT_READY)             \
    ITEM(EEXIST)                    \
    ITEM(ENOPATH)                   \
    ITEM(ENAMETOOLONG)              \
    ITEM(EACCES)                    \
    ITEM(EFR_EXIST)                 \
    ITEM(EFR_INVALID_OBJECT)        \
    ITEM(EFR_WRITE_PROTECTED)       \
    ITEM(EFR_INVALID_DRIVE)         \
    ITEM(EFR_NOT_ENABLED)           \
    ITEM(EFR_NO_FILESYSTEM)         \
    ITEM(EFR_MKFS_ABORTED)          \
    ITEM(EFR_TIMEOUT)               \
    ITEM(EFR_LOCKED)                \
    ITEM(EFR_NOT_ENOUGH_CORE)       \
    ITEM(EMFILE)                    \
    ITEM(EINVAL)                    /* (19) */\
\
    ITEM(EAGAIN)                       /* -1 ... */    \
    ITEM(EMSGSIZE)                  \
    ITEM(ECONNECT_FAIL)             \
    ITEM(EBUFFER_SIZE_NOT_POWER_2)  \
    ITEM(EALREADY_CLOSED)           \
    ITEM(ESOCKET_CLOSED)            \
    ITEM(ESOCKET_WRONG_TYPE)        \
    ITEM(ESOCKET_NO_DIRECTION)      \
    ITEM(ECONNECT_FAIL_WRONG_PORT)  \
    ITEM(ECONNECT_FAIL_WRONG_TYPE)  \
    ITEM(ECOPY_NEEDED)              \
    ITEM(EUNSUPPORTED)              \
    ITEM(EIN_PROXY)                 \
    ITEM(ENO_DATA_BUFFER)           \
    ITEM(EALREADY_CONNECTED)        \
    ITEM(ENOT_CONNECTED)            \
    ITEM(EOOB)                      \
    ITEM(EBAD_FLAGS)                \
    ITEM(EIN_JOIN)                  \
    ITEM(EUSER_FULFILL_ERROR)       \
    ITEM(EAUTH_TOKEN_ERROR)         \
    ITEM(EBAD_RESERVATION)          \
    ITEM(EBAD_SEAL)                   /* ... -23 */       \
    ITEM(EOPNOTSUPP)                \
    ITEM(ENOSPC)                    \
    ITEM(EXDEV)                     \
    ITEM(ENOPROTOOPT)               \
    ITEM(EADDRINUSE)                \
    ITEM(ECONNRESET)                \
    ITEM(ENOTCONN)                  \
    ITEM(EPIPE)                     \
    ITEM(ETIMEDOUT)                 \
    ITEM(ECONNREFUSED)              \
    ITEM(ENETDOWN)                  \
    ITEM(ENETUNREACH)               \
    ITEM(EHOSTDOWN)                 \
    ITEM(EHOSTUNREACH)              \
    ITEM(EINPROGRESS)               \
    ITEM(ECONNABORTED)              \
    ITEM(EINTR)                     \
    ITEM(ENOTDIR)                   \
    ITEM(ELOOP)

DECLARE_ENUM(err_e, ER_LIST)

extern int errno;

static int map_fs_errors(FRESULT fresult) {
    if(fresult != FR_OK) {
        errno = (int)fresult; // Organised to match
        return -1;
    }
    return 0;
}

static ssize_t map_sock_errors(ssize_t sock_er) {
    if(sock_er < 0) {
        errno = (int)((-sock_er) + (EAGAIN-1));
        return -1;
    }
    return sock_er;
}

#endif