/* fixme: dummy header for zlib */

#ifndef __DUMMY_H
#define __DUMMY_H

#include "stdlib.h"
#include "stdio.h"

#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"
#pragma clang diagnostic ignored "-Wmacro-redefined"

static inline int open(const char *path, int oflag, ... ) {
	panic("open");
}

static inline int read( int handle, void *buffer, int nbyte ) {
	panic("read");
}

static inline off_t lseek(int fd, off_t offset, int whence) {
	panic("lseek");
}

static inline ssize_t write(int fd, const void *buf, size_t count) {
	panic("write");
}

static inline int close(int fildes) {
	panic("close");
}

int open(const char *path, int oflag, ... );
int read( int handle, void *buffer, int nbyte );
off_t lseek(int fd, off_t offset, int whence);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fildes);
#define O_WRONLY 0
#define O_CREAT 0
#define O_TRUNC 0
#define O_APPEND 0
#define O_RDONLY 0

int sprintf ( char * str, const char * format, ... );

#endif
