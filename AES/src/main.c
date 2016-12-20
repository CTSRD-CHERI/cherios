
 /*
   -----------------------------------------------------------------------
   Copyright (c) 2001 Dr Brian Gladman <brg@gladman.uk.net>, Worcester, UK
   
   TERMS

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   This software is provided 'as is' with no guarantees of correctness or
   fitness for purpose.
   -----------------------------------------------------------------------
 */

/* Example of the use of the AES (Rijndael) algorithm for file  */
/* encryption.  Note that this is an example application, it is */
/* not intended for real operational use.  The Command line is: */
/*                                                              */
/* aesxam input_file_name output_file_name [D|E] hexadecimalkey */
/*                                                              */
/* where E gives encryption and D decryption of the input file  */
/* into the output file using the given hexadecimal key string  */
/* The later is a hexadecimal sequence of 32, 48 or 64 digits   */
/* Examples to encrypt or decrypt aes.c into aes.enc are:       */
/*                                                              */
/* aesxam file.c file.enc E 0123456789abcdeffedcba9876543210    */
/*                                                              */
/* aesxam file.enc file2.c D 0123456789abcdeffedcba9876543210   */
/*                                                              */
/* which should return a file 'file2.c' identical to 'file.c'   */

#include<mips.h>
#include<stdio.h>
#include<stdlib.h>
//#include<memory.h>
#include<ctype.h>
#include<stdint.h>
#include<string.h> // for memcpy();
#include<misc.h> // for countof();
#include<object.h>
#include<namespace.h>

#include"aes.h"

int main_aes(byte *in, byte *out, int64_t length, char *givenKey);

void (*msg_methods[]) = {main_aes};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

/* A Pseudo Random Number Generator (PRNG) used for the     */
/* Initialisation Vector. The PRNG is George Marsaglia's    */
/* Multiply-With-Carry (MWC) PRNG that concatenates two     */
/* 16-bit MWC generators:                                   */
/*     x(n)=36969 * x(n-1) + carry mod 2^16                 */ 
/*     y(n)=18000 * y(n-1) + carry mod 2^16                 */
/* to produce a combined PRNG with a period of about 2^60.  */  
/* The Pentium cycle counter is used to initialise it. This */
/* is crude but the IV does not need to be secret.          */
 
/* void cycles(unsigned long *rtn)     */
/* {                           // read the Pentium Time Stamp Counter */
/*     __asm */
/*     { */
/*     _emit   0x0f            // complete pending operations */
/*     _emit   0xa2 */
/*     _emit   0x0f            // read time stamp counter */
/*     _emit   0x31 */
/*     mov     ebx,rtn */
/*     mov     [ebx],eax */
/*     mov     [ebx+4],edx */
/*     _emit   0x0f            // complete pending operations */
/*     _emit   0xa2 */
/*     } */
/* } */

#define RAND(a,b) (((a = 36969 * (a & 65535) + (a >> 16)) << 16) + (b = 18000 * (b & 65535) + (b >> 16))  )

void fillrand(byte *buf, int len)
{   static uint32_t a[2], mt = 1, count = 4;
    static uint8_t          r[4];
    int                  i;

    if(mt) { 
	 mt = 0; 
	 /*cycles(a);*/
      a[0]=0xeaf3;
	 a[1]=0x35fe;
    }

    for(i = 0; i < len; ++i)
    {
        if(count == 4)
        {
            *(uint32_t*)r = RAND(a[0], a[1]);
            count = 0;
        }

        buf[i] = r[count++];
    }
}    

uint64_t encfile(byte *fin, byte *fout, aes *ctx, uint64_t length) {   
    byte            inbuf[16], outbuf[16];
    uint64_t   i=0, l=0;
    uint64_t readbytes = 0, writebytes = 0;

    fillrand(outbuf, 16);           /* set an IV for CBC mode           */
    memcpy(fout + writebytes, outbuf, 16);
    writebytes += 16;
    fillrand(inbuf, 1);             /* make top 4 bits of a byte random */
    l = 15;                         /* and store the length of the last */
                                    /* block in the lower 4 bits        */
    inbuf[0] = ((char)length & 15) | (inbuf[0] & ~15);

    while(readbytes < length) {
        /* input 1st 16 bytes to buf[1..16] */
        i = length - readbytes;
        memcpy(inbuf + 16 - l, fin + readbytes, (i<l)? i:l);
        readbytes += (i<l)? i:l;
        if(i < l) break;            /* if end of the input file reached */

        for(i = 0; i < 16; ++i)         /* xor in previous cipher text  */
            inbuf[i] ^= outbuf[i]; 

        encrypt(inbuf, outbuf, ctx);    /* and do the encryption        */

        memcpy(fout + writebytes, outbuf, 16);
        writebytes += 16;
                                    /* in all but first round read 16   */
        l = 16;                     /* bytes into the buffer            */
        i = 0;
    }

    /* except for files of length less than two blocks we now have one  */
    /* byte from the previous block and 'i' bytes from the current one  */
    /* to encrypt and 15 - i empty buffer positions. For files of less  */
    /* than two blocks (0 or 1) we have i + 1 bytes and 14 - i empty    */
    /* buffer position to set to zero since the 'count' byte is extra   */

    if(l == 15)                         /* adjust for extra byte in the */
        ++i;                            /* in the first block           */

    if(i)                               /* if bytes remain to be output */
    {
        while(i < 16)                   /* clear empty buffer positions */
            inbuf[i++] = 0;
     
        for(i = 0; i < 16; ++i)         /* xor in previous cipher text  */
            inbuf[i] ^= outbuf[i]; 

        encrypt(inbuf, outbuf, ctx);    /* encrypt and output it        */

        memcpy(fout + writebytes, outbuf, 16);
        writebytes += 16;
    }
        
    return writebytes;
}

int decfile(byte *fin, byte *fout, aes *ctx, uint64_t length) {
    byte    inbuf1[16], inbuf2[16], outbuf[16], *bp1, *bp2, *tp;
    uint64_t i, l, flen;
    uint64_t readbytes = 0, writebytes = 0;

    if(length < 32) {
        printf("File to be decrypt is corrupt\n");
        return -1;
    }

    memcpy(inbuf1, fin + readbytes, 16);
    readbytes += 16;

    memcpy(inbuf2, fin + readbytes, 16);
    readbytes += 16;

    decrypt(inbuf2, outbuf, ctx);   /* decrypt it                       */

    for(i = 0; i < 16; ++i)         /* xor with previous input          */
        outbuf[i] ^= inbuf1[i];

    flen = outbuf[0] & 15;  /* recover length of the last block and set */
    l = 15;                 /* the count of valid bytes in block to 15  */                              
    bp1 = inbuf1;           /* set up pointers to two input buffers     */
    bp2 = inbuf2;

    while(1)
    {
        i = length - readbytes;
        memcpy(bp1, fin + readbytes, (i<16)? i:16);
        readbytes += (i<16)? i:16;
        if(i < 16) break;

        /* if a block has been read the previous block must have been   */
        /* full lnegth so we can now write it out                       */
         
        memcpy(fout + writebytes, outbuf + 16 - l, l);
        writebytes += l;

        decrypt(bp1, outbuf, ctx);  /* decrypt the new input block and  */

        for(i = 0; i < 16; ++i)     /* xor it with previous input block */
            outbuf[i] ^= bp2[i];
        
        /* set byte count to 16 and swap buffer pointers                */

        l = i; tp = bp1, bp1 = bp2, bp2 = tp;
    }

    /* we have now output 16 * n + 15 bytes of the file with any left   */
    /* in outbuf waiting to be output. If x bytes remain to be written, */
    /* we know that (16 * n + x + 15) % 16 = flen, giving x = flen + 1  */
    /* But we must also remember that the first block is offset by one  */
    /* in the buffer - we use the fact that l = 15 rather than 16 here  */  

    l = (l == 15 ? 1 : 0); flen += 1 - l;

    if(flen) {
        memcpy(fout + writebytes, outbuf + l, flen);
        writebytes += flen;
    }

    return writebytes;
}

int
main_aes(byte *in, byte *out, int64_t length, char *givenKey) {   
    char    *cp, ch;
    byte key[32];
    int     i=0, by=0, key_len=0, err = 0;
    aes     ctx[1];

    cp = givenKey;   /* this is a pointer to the hexadecimal key digits  */
    i = 0;          /* this is a count for the input digits processed   */
    
    while(i < 64 && *cp)    /* the maximum key length is 32 bytes and   */
    {                       /* hence at most 64 hexadecimal digits      */
        ch = *cp++;            /* process a hexadecimal digit  */
        if(ch >= '0' && ch <= '9')
            by = (by << 4) + ch - '0';
        else if(ch >= 'A' && ch <= 'F')
            by = (by << 4) + ch - 'A' + 10;
        else                            /* error if not hexadecimal     */
        {
            printf("key must be in hexadecimal notation\n"); 
            err = -2; goto exit;
        }
        
        /* store a key byte for each pair of hexadecimal digits         */
        if(i++ & 1) 
            key[i / 2 - 1] = by & 0xff; 
    }

    if(*cp)
    {
        printf("The key value is too long\n"); 
        err = -3; goto exit;
    }
    else if(i < 32 || (i & 15))
    {
        printf("The key length must be 32, 48 or 64 hexadecimal digits\n");
        err = -4; goto exit;
    }

    key_len = i / 2;

    if(length > 0) {  
        /* encryption in Cipher Block Chaining mode */
        set_key(key, key_len, enc, ctx);

        err = encfile(in, out, ctx, length);
    } else {                           /* decryption in Cipher Block Chaining mode */
        set_key(key, key_len, dec, ctx);
    
        err = decfile(in, out, ctx, -length);
    }
exit:   
    return err;
}

int main() {
    printf("AES Hello World.\n");
    int ret = namespace_register(5, act_self_ref, act_self_id);
    if(ret!=0) {
        printf("AES: register failed\n");
        return -1;
    }
    printf("AES: register OK\n");
    msg_enable = 1;
    return 0;
}
