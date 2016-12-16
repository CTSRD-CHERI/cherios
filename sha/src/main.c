/* NIST Secure Hash Algorithm */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <time.h>
#include "sha.h"

extern char __input_begin, __input_end;

int main()
{
    SHA_INFO sha_info;
    size_t fin_size = (size_t)(&__input_end - &__input_begin);
    char *fin = &__input_begin;

    sha_stream(&sha_info, fin, fin_size);
    sha_print(&sha_info);
    return(0);
}
