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

// This layer provides slightly more abstract file handling than raw sockets.
// Unistd wraps it again to look fully posix-y (but you should avoid using that if possible).

#ifndef CHERIOS_CHERISTD_H
#define CHERIOS_CHERISTD_H

#include "sockets.h"
#include "stdio.h"
// Use the same types as the (currently) only filesystem.
#include "ff.h"
#define LWIP_SOCKET_TYPES           1
#define LWIP_SOCKET                 0
// We steal some type definitions from LWIP. We probably should not.
//#include "lwip/sockets.h"

typedef FILE* FILE_t;
typedef capability dir_token_t;


// These are also unistd but are the same in CheriOS

FRESULT stat(const char* path, FILINFO* fno);
FRESULT readdir(dir_token_t dir, FILINFO* fno);
FRESULT closedir(dir_token_t dir);
FRESULT mkdir(const char* name);
FRESULT rename(const char* old, const char* __new);
FRESULT unlink(const char* name);

// Open/close will contact the FS activation for you and also do allocation for you
// ONLY use close on something created by open. Otherwise the other functions are just wrappers for the socket_* family

__BEGIN_DECLS

DEC_ERROR_T(FILE_t);

ERROR_T(FILE_t) open_er(const char* name, int mode, enum SOCKET_FLAGS flags, const uint8_t* key, const uint8_t* iv);

static inline FILE_t open_file(const char* name, int mode, enum SOCKET_FLAGS flags) {
    ERROR_T(FILE_t) res = open_er(name, mode, flags, NULL, NULL);
    return IS_VALID(res) ? res.val : NULL;
}
static inline FILE_t open_encrypted(const char* name, int mode, enum SOCKET_FLAGS flags, const uint8_t* key, const uint8_t* iv) {
    ERROR_T(FILE_t) res = open_er(name, mode, flags, key, iv);
    return IS_VALID(res) ? res.val : NULL;
}

ssize_t close_file(FILE_t file);
ssize_t lseek_file(FILE_t file, int64_t offset, int whence);

#define write_file(file,buf,length) socket_send(file,(const char*)buf,length,MSG_NONE)
#define read_file(file,buf,length) socket_recv(file,(char*)buf,length,MSG_NONE)

#define sendfile(sockout,sockin,count) socket_sendfile(sockout,sockin,count);

void process_async_closes(int force);
void needs_drb(FILE_t file);

ssize_t truncate_file(FILE_t file);
ssize_t flush_file(FILE_t file);
ssize_t filesize(FILE_t file);

act_kt try_get_fs(void);
dir_token_t opendir(const char* name);


__END_DECLS


#endif //CHERIOS_CHERISTD_H
