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
#ifndef CHERIOS_UNISTD_H
#define CHERIOS_UNISTD_H

#include "sockets.h"
#include "stdio.h"
#include "ff.h"

typedef FILE* FILE_t;
typedef capability dir_token_t;

// Open/close will contact the FS activation for you and also do allocation for you
// ONLY use close on something created by open. Otherwise the other functions are just wrappers for the socket_* family

FILE_t open(const char* name, int mode, enum SOCKET_FLAGS flags);
ssize_t close(FILE_t file);
#define write(file,buf,length) socket_send(file,buf,length,0)
#define read(file,buf,length) socket_recv(file,buf,length,0)
ssize_t lseek(FILE_t file, int64_t offset, int whence);
#define sendfile(sockout,sockin,count) socket_sendfile(sockout,sockin,count)
int mkdir(const char* name);
int rename(const char* old, const char* new);
ssize_t truncate(FILE_t file);
ssize_t flush(FILE_t file);
ssize_t filesize(FILE_t file);
act_kt try_get_fs(void);
int stat(const char* path, FILINFO* fno);

dir_token_t opendir(const char* name);
FRESULT readdir(dir_token_t dir, FILINFO* fno);
FRESULT closedir(dir_token_t dir);

#endif //CHERIOS_UNISTD_H
