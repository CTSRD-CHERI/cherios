/*-
 * Copyright (c) 2020 Lawrence Esswood
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
#ifndef CHERIOS_TIME_H
#define CHERIOS_TIME_H

#include "mips.h"
#include "cdefs.h"
#include "syscalls.h"

typedef struct tm {
    int tm_sec;         /* seconds,  range 0 to 59          */
    int tm_min;         /* minutes, range 0 to 59           */
    int tm_hour;        /* hours, range 0 to 23             */
    int tm_mday;        /* day of the month, range 1 to 31  */
    int tm_mon;         /* month, range 0 to 11             */
    int tm_year;        /* The number of years since 1900   */
    int tm_wday;        /* day of the week, range 0 to 6    */
    int tm_yday;        /* day in the year, range 0 to 365  */
    int tm_isdst;       /* daylight saving time             */
    int tm_gmtoff;
} tm;

typedef uint64_t clock_t;
typedef uint64_t time_t;
typedef uint64_t suseconds_t;

typedef struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
} timeval;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

// Not implemented

__BEGIN_DECLS

static void gettimeofday(struct timeval* tv, __unused void* timezone) {
    register_t cycles = syscall_now();
    register_t us = CLOCK_TO_US(cycles);
    register_t s = us / (1000 * 1000);
    us -= (1000 * 1000 * s);
    tv->tv_sec = s;
    tv->tv_usec = us;
}


clock_t clock();
double difftime(time_t time1, time_t time0);
time_t mktime(tm* timeptr);
time_t time(time_t* timer);
char* asctime(const tm* timeptr);
char* ctime(const time_t* timer);
tm*    gmtime(const time_t* timer);
tm* localtime(const time_t* timer);
size_t strftime(char* __restrict s, size_t maxsize, const char* __restrict format,
                const tm* __restrict timeptr);

__END_DECLS

#endif //CHERIOS_TIME_H
