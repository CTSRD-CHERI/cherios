#include<stdio.h>
#include<object.h>
#include<namespace.h>
#include<stdlib.h>
#include<mips.h>
#include<assert.h>
#include<sha_info.h>

extern char __AES_start, __AES_end;

int
main() {
    for(int i=0; i<8; i++) {
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }

	void * u_ref = namespace_get_ref(5);
	assert(u_ref != NULL);
	void * u_id  = namespace_get_id(5);

	void * sha_ref = namespace_get_ref(6);
	assert(sha_ref != NULL);
	void * sha_id  = namespace_get_id(6);

    int64_t len = &__AES_end - &__AES_start;

    uint8_t *enc = (uint8_t *)malloc((size_t)len+32);
    uint8_t *encdec = (uint8_t *)malloc((size_t)len+32);

    const char *theKey = "0123456789ABCDEFFEDCBA98765432100123456789ABCDEFFEDCBA9876543210";

    int64_t encret = ccall_rrrr_r(u_ref, u_id, 0, (register_t)&__AES_start, (register_t)enc, len, (register_t)theKey);
    printf("Encryption length: %ld, ended with %ld status.\n", len, encret);

    uint64_t decret = ccall_rrrr_r(u_ref, u_id, 0, (register_t)enc, (register_t)encdec, -encret, (register_t)theKey);
    printf("Decryption ended with %ld status.\n", decret);

    SHA_INFO theinfo;
    ccall_rrr_n(sha_ref, sha_id, 0, (register_t)&theinfo, (register_t)&__AES_start, (size_t)len);
    ccall_r_n(sha_ref, sha_id, 1, (register_t)&theinfo);

    return 0;
}
