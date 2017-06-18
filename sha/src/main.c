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
    printf("SHA Hello World.\n");
    int ret = namespace_register(6, act_self_ref, act_self_id);
    if(ret!=0) {
        printf("SHA: register failed\n");
        return -1;
    }
    ret = namespace_dcall_register(6, act_self_msg, act_self_base);
    if(ret!=0) {
        printf("SHA: DCALL register failed\n");
        return -1;
    }
    printf("SHA: register OK\n");
    msg_enable = 1;
    return(0);
}
