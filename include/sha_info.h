#ifndef SHA_INFO_H
#define SHA_INFO_H

#include<mips.h>

/* NIST Secure Hash Algorithm */
/* heavily modified from Peter C. Gutmann's implementation */

/* Useful defines & typedefs */

typedef uint8_t BYTE;
typedef uint32_t LONG;
typedef struct {
    LONG digest[5];		/* message digest */
    LONG count_lo, count_hi;	/* 64-bit bit count */
    LONG data[16];		/* SHA data buffer */
} SHA_INFO;

#endif /* SHA_H */
