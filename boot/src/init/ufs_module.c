/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 * Copyright (c) 2014 Nathan Whitehorn
 * All rights reserved.
 * Copyright (c) 2015 Eric McCorkle
 * All rights reverved.
 * Copyright (c) 2016 Hadrien Barral
 * All rights reverved.
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
 *
 * $FreeBSD$
 */

#include <elf.h>
#include "init.h"
#include "debug.h"
#include "stdio.h"
#include "assert.h"
#include "ufs_read.h"

extern u8 __fs_start, __fs_end;

int dskread(u8 *buf, u_int64_t lba, int nblk) {
	size_t size  = nblk * DEV_BSIZE;
	size_t start = lba  * DEV_BSIZE;

	u8 * fsp = &__fs_start;
	for(size_t i=0; i<size; i++) {
		// Check read is not out of bounds
		assert(fsp + start + i < &__fs_end);
		buf[i] = fsp[start + i];
	}
	return 0;
}

void *
load(const char *filepath, int *bufsize)
{
	ufs_ino_t ino;
	size_t size;
	ssize_t read;

	printf("Loading '%s'\n", filepath);

	if ((ino = lookup(filepath)) == 0) {
		printf("Failed to lookup '%s' (file not found?)\n", filepath);
		return NULL;
	}

	if (fsread_size(ino, NULL, 0, &size) < 0 || size <= 0) {
		printf("Failed to read size of '%s' ino: %d\n", filepath, ino);
		return NULL;
	}

	void * buf = env.alloc(size, &env).data;
	if (buf == NULL) {
		printf("Failed to allocate read buffer %zu for '%s'\n",
		       size, filepath);
		return NULL;
	}

	read = fsread(ino, buf, size);
	if ((size_t)read != size) {
		printf("Failed to read '%s' (%zd != %zu)\n", filepath, read,
		       size);
		env.free(buf, &env);
		return NULL;
	}

	printf(KWHT"'%s' loaded\n", filepath);

	*bufsize = (int)size;

	return buf;
}
