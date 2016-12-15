/* +++Date last modified: 05-Jul-1997 */

/*
 **  BITCNTS.C - Test program for bit counting functions
 **
 **  public domain by Bob Stout & Auke Reitsma
 */

#include<mips.h>
#include <stdio.h>
#include <stdlib.h>
#include "conio.h"
#include <limits.h>
#include "bitops.h"

#define FUNCS  7
#define ITERATIONS 500000

static int CDECL bit_shifter(uint32_t x);

int main()
{
    uint32_t i, j, n, seed;
    uint32_t iterations;
    static int (* CDECL pBitCntFunc[FUNCS])(uint32_t) = {
        bit_count,
        bitcount,
        ntbl_bitcnt,
        ntbl_bitcount,
        /*            btbl_bitcnt, DOESNT WORK*/
        BW_btbl_bitcount,
        AR_btbl_bitcount,
        bit_shifter
    };
    static const char *text[FUNCS] = {
        "Optimized 1 bit/loop counter",
        "Ratko's mystery algorithm",
        "Recursive bit count by nybbles",
        "Non-recursive bit count by nybbles",
        /*            "Recursive bit count by bytes",*/
        "Non-recursive bit count by bytes (BW)",
        "Non-recursive bit count by bytes (AR)",
        "Shift and count bits"
    };

    iterations=ITERATIONS;

    puts("Bit counter algorithm benchmark\n");

    for (i = 0; i < FUNCS; i++) {
        for (j = n = 0, seed = 0; j < iterations; j++, seed += 13)
            n += pBitCntFunc[i](seed);
        printf("Counting algorithm %s counts:\n    %d.\n", text[i], n);
    }
    return 0;
}

static int CDECL bit_shifter(uint32_t x)
{
    int i, n;

    for (i = n = 0; x && (i < (int)(sizeof(uint32_t) * CHAR_BIT)); ++i, x >>= 1)
        n += (int)(x & 1L);
    return n;
}
