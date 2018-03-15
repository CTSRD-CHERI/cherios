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

#include <queue.h>
#include "lib.h"
#include "virtioblk.h"
#include "mman.h"
#include "nano/usernano.h"

void rw_asyc_test(char* buf1, char* buf2) {
	virtio_blk_init();
	for(int i = 0; i < 0x200; i++) {
		buf1[i] = 1;
		buf2[i] = 2;
	}

	// Make some data

	virtio_write(buf1, 1);
	virtio_write(buf2, 2);

	for(int i = 0; i < 0x200; i++) {
		buf1[i] = 0;
		buf2[i] = 0;
	}

	// Read it back in an async way

	virtio_async_read(buf1, 1, 1, 77);
	virtio_async_read(buf2, 2, 2, 77);

	for(int j = 0; j != 2; j++) {
		msg_t* m = get_message();

		printf("Returnm msg!!\n");
		assert(m->v0 == 77);

		register_t read_no = m->a1;
		register_t res = m->a0;

		assert(res == 0);

		char* buf = read_no == 1 ? buf1 : buf2;
		char c = read_no == 1 ? 1 : 2;

		for(int i = 0; i < 0x200; i++) {
			assert(buf[i] == c);
		}

		next_msg();
	}
}

void rw_block_test(char* buf) {
	virtio_blk_init();

	for(int i = 0; i < 0x200; i++) {
		buf[i] = (char)i;
	}

	virtio_write(buf, 1);

	for(int i = 0; i < 0x200; i++) {
		buf[i] = 0;
	}

	virtio_read(buf, 1);

	int changed = 0;
	for(int i = 0; i < 0x200; i++) {
		if(buf[i] != (char)i) {
			changed = 1;
		}
	}

	assert(!changed);
}

char* malloc_stupid_buffer(void) {
	char* buf1 = (char*)malloc(UNTRANSLATED_PAGE_SIZE * 2);
	char* buf2 = (char*)malloc(MEM_REQUEST_MIN_REQUEST);
	buf2[0] = 1; // touch this to make sure the page is touched
	buf1[UNTRANSLATED_PAGE_SIZE] = 1; // touch this one now
	return buf1 + MEM_REQUEST_MIN_REQUEST - 0x100;
}

char* malloc_non_stupid(void) {
	return (char*)malloc(MEM_REQUEST_MIN_REQUEST);
}

int main(capability fs_cap) {
	printf("Fatfs: Hello world\n");

	/* Init virtio-blk session */
	virtio_blk_session(fs_cap);

	FRESULT res;

	FATFS fs;

    int already_existed;

	if((res = f_mount(&fs, "", 1)) != FR_NO_FILESYSTEM && res != 0) {
		printf("MT:%d\n", res);
		goto end;
	}

    if(res == FR_NO_FILESYSTEM) {
        already_existed = 0;
        if((res = f_mkfs("", 0, 0))) {
            printf("MK:%d\n", res);
            goto end;
        }
    } else {
        already_existed = 1;
    }


	FIL f;
	FIL * fp = &f;
	const char * path = "aaaa";
    const char data[] = "This string written to file!";
    u_int count = 0;

    if(!already_existed) {
        if((res = f_open(fp, path, FA_WRITE | FA_CREATE_NEW))) {
            printf("OP:%d\n", res);
            goto end;
        }



        if((res = f_write(fp, data, sizeof(data), &count))) {
            printf("WR:%d\n", res);
        }

        if((res = f_close(fp))) {
            printf("CL:%d\n", res);
            goto end;
        }
    }

	if((res = f_open(fp, path, FA_READ))) {
		printf("OPR:%d\n", res);
		goto end;
	}

	char read[sizeof(data)+1];
	if((res = f_read(fp, read, sizeof(data), &count))) {
		printf("RD:%d\n", res);
		goto end;
	}
	read[sizeof(data)] = '\0';

	printf("FatFS: -%s- (%x) \n", read, count);
	return 0;

	end:
	printf("FatFS self-test failed\n");

	return -1;
}
