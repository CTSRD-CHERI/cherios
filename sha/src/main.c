/* NIST Secure Hash Algorithm */

#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<misc.h>
#include<object.h>
#include<namespace.h>
#include "sha.h"

void (*msg_methods[]) = {sha_stream, sha_print};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL};
size_t ctrl_methods_nb = countof(ctrl_methods);

int main()
{
    int ret = namespace_register(6, act_self_ref, act_self_id);
    if(ret!=0) {
        printf("SHA: register failed\n");
        return -1;
    }
    msg_enable = 1;
    printf("SHA Hello World.\n");
    return(0);
}
