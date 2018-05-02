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

#include "assert.h"
#include "stdlib.h"
#include "zlib.h"
#include "object.h"
#include "misc.h"
#include "stdio.h"
#include "namespace.h"
#include "string.h"

#define MAX_CHUNK 0x4000
static size_t CHUNK = 0;

int oid = 0;
void * new_identifier(void) {
	int * object = malloc(sizeof(int));
	assert(object != NULL);
	*object = oid++;
    //TODO use a seal manager
	return object;
}

void (*msg_methods[]) = {deflateInit_, deflate, deflateEnd};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, new_identifier, dtor_null};
size_t ctrl_methods_nb = countof(ctrl_methods);

static int ferror ( FILE * stream __unused) {
	return 0;
}

static int fputs ( const char * str, FILE * stream ) {
	stream = NULL;
	puts(str);
	return 0;
}

static FILE * stdin = NULL;
static FILE * stdout = NULL;

#define BUFINLEN 0x10000
static size_t bufinlen = BUFINLEN;
static uint8_t bufin[BUFINLEN];
static size_t nbufin = 0;
static uint8_t bufout[0x1000];
static size_t nbufout = 0;
static size_t lastpnbufin = 0;

static size_t fread (__unused void * ptr, size_t size, size_t count, FILE * stream ) {
	assert(stream == stdin);
	assert(size == 1);
	size_t readlen  = imin(bufinlen-nbufin, count);
	//memcpy(ptr, bufin+nbufin, readlen);
	nbufin += readlen;
	assert(nbufin <= bufinlen);
	if(nbufin - lastpnbufin > 0x100000) {
		lastpnbufin = nbufin;
		printf("%lu\n", nbufin);
	}
	return readlen;
}

static size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream ) {
	assert(stream == stdout);
	memcpy(bufout+nbufout, ptr, size*count);
	//nbufout +=  size*count;
	return count;
}

static int feof ( FILE * stream ) {
	stream = NULL;
	return nbufin >= bufinlen;
}

static void dispout(FILE * stream) {
	assert(stream == stdout);
	for(size_t i=0; i<nbufout; i++) {
		printf("%02x", bufout[i]);
	}
	printf(" \n");
}

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
int def(FILE *source, FILE *dest, int level)
{
    printf("def\n");
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[MAX_CHUNK];
    unsigned char out[MAX_CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, level);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf(FILE *source, FILE *dest)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[MAX_CHUNK];
    unsigned char out[MAX_CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

/* report a zlib or i/o error */
void zerr(int ret)
{
    fputs("zpipe: ", stderr);
    switch (ret) {
    case Z_ERRNO:
        if (ferror(stdin))
            fputs("error reading stdin\n", stderr);
        if (ferror(stdout))
            fputs("error writing stdout\n", stderr);
        break;
    case Z_STREAM_ERROR:
        fputs("invalid compression level\n", stderr);
        break;
    case Z_DATA_ERROR:
        fputs("invalid or incomplete deflate data\n", stderr);
        break;
    case Z_MEM_ERROR:
        fputs("out of memory\n", stderr);
        break;
    case Z_VERSION_ERROR:
        fputs("zlib version mismatch!\n", stderr);
    }
}

register_t cp0_count_get(void)
{
	register_t count;

	__asm__ __volatile__ ("dmfc0 %0, $9" : "=r" (count));
	return (count & 0xFFFFFFFF);
}


void selftest(size_t chunk) {
	assert(chunk <= MAX_CHUNK);
	CHUNK = chunk;
	nbufin = 0;
	nbufout = 0;
	lastpnbufin = 0;

	int argc = 1;
	int ret = 0;
	register_t count = cp0_count_get();
	__asm("li $0, 0x1337");

	memset(bufin, 0, bufinlen);
//	bufin[0] = 'A';
	bufinlen = 512*1024;
	bufinlen = 4*1024*1024;

	/* do compression if no arguments */
	if (argc == 1) {
		ret = def(stdin, stdout, Z_DEFAULT_COMPRESSION);
	} else {
		ret = inf(stdin, stdout);
	}
	if (ret != Z_OK) {
		zerr(ret);
		syscall_puts("zlib: error\n");
	} else {
		dispout(stdout);
	}
	__asm("li $0, 0x1337");
	printf("zlib selftest %04lx done in %lx (%lx %lx)\n",
	       chunk, cp0_count_get()-count, count, cp0_count_get());
}

/* compress or decompress from stdin to stdout */
int main(void)
{
	printf("zlib: Hello world\n");

	/* Register ourself to the kernel as being the zlib module */
	int ret = namespace_register(namespace_num_zlib, act_self_ref);
	if(ret!=0) {
		printf("zlib: register failed\n");
		return -1;
	}

	#if 0
	for(int i=0; i<10; i++) {
		selftest(1<<(5+i));
	}
	#endif

	printf("zlib: Going into daemon mode\n");
	msg_enable = 1; /* Go in waiting state instead of exiting */
	return 0;
}
