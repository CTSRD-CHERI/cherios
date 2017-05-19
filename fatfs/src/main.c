/*-
 * Copyright (c) 2016 Hadrien Barral
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

#include "lib.h"

int main(capability fs_cap) {
	printf("Fatfs: Hello world\n");

	/* Init virtio-blk session */
	virtio_blk_session(fs_cap);

	FRESULT res;

	FATFS fs;
	if((res = f_mount(&fs, "", 1)) != FR_NO_FILESYSTEM) {
		printf("MT:%d\n", res);
		goto end;
	}

	if((res = f_mkfs("", 0, 0))) {
		printf("MK:%d\n", res);
		goto end;
	}

	FIL f;
	FIL * fp = &f;
	const char * path = "aaaa";
	if((res = f_open(fp, path, FA_WRITE | FA_CREATE_NEW))) {
		printf("OP:%d\n", res);
		goto end;
	}

	const char data[] = "test42";
	u_int count = 0;
	if((res = f_write(fp, data, 7, &count))) {
		printf("WR:%d\n", res);
	}

	if((res = f_close(fp))) {
		printf("CL:%d\n", res);
		goto end;
	}

	if((res = f_open(fp, path, FA_READ))) {
		printf("OPR:%d\n", res);
		goto end;
	}

	char read[0x10];
	if((res = f_read(fp, read, sizeof(data), &count))) {
		printf("RD:%d\n", res);
		goto end;
	}
	read[sizeof(data)] = '\0';

	printf("FatFS: -%s-\n", read);
	return 0;

	end:
	printf("FatFS self-test failed\n");

	return -1;
}
