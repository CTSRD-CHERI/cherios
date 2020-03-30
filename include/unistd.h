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

// Compatibility layer for POSIX.

#ifndef CHERIOS_UNISTD_H
#define CHERIOS_UNISTD_H

#include "cheristd.h"
#include "ff.h"
#include "errno.h"

typedef int64_t off_t;

// Low short has modes supported by FS.
// High short covers the socket flags and extra functions calls after open.

#define O_APPEND                    ((1 << 16) | FA_WRITE)
#define O_CREAT                     FA_OPEN_ALWAYS
#define O_RDWR                      (FA_READ | FA_WRITE)
#define O_RDONLY                    FA_READ
#define O_WRONLY                    FA_WRITE
#define O_NONBLOCK                  (1 << 17)
#define O_TRUNC                     ((1 << 18))
#define O_NDELAY                    O_NONBLOCK /* same as O_NONBLOCK, for compatibility */

static int open(const char *name, int flags) {

    u_long nonblock = flags & O_NONBLOCK;

    int mode = (int)(flags & 0xFF);

    ERROR_T(FILE_t) result = open_er(name, mode,
                                     (enum SOCKET_FLAGS)((nonblock ? MSG_DONT_WAIT : MSG_NONE) | SOCKF_GIVE_SOCK_N | MSG_BUFFER_WRITES),
                                     NULL, NULL);

    if(IS_VALID(result)) {
        ssize_t res = 0;
        if(flags & O_TRUNC) {
            res = truncate_file(result.val);
        }
        if(res < 0) {
            map_sock_errors(res);
            return -1;
        }
        if((flags & O_APPEND) == O_APPEND) {
            res = lseek_file(result.val, 0, SEEK_END);
        }
        if(res < 0) {
            map_sock_errors(res);
            return -1;
        }
    } else {
        if((ssize_t)result.er < 0) map_sock_errors((ssize_t)result.er);
        else map_fs_errors((FRESULT)result.er);
        return -1;
    }

    return socket_to_posix_handle(result.val);
}

static inline int close(int handle) {
    return map_sock_errors(close_file(posix_handle_to_socket(handle)));
}

static inline ssize_t lseek(int handle, int64_t offset, int whence) {
    return lseek_file(posix_handle_to_socket(handle), offset, whence);
}

static inline ssize_t write(int handle, const void *buf, size_t length) {
    return write_file(posix_handle_to_socket(handle), buf, length);
}

static inline ssize_t read(int handle, void *buf, size_t length) {
    return read_file(posix_handle_to_socket(handle), buf, length);
}

#endif //CHERIOS_UNISTD_H
