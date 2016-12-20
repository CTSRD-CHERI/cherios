#include<stdio.h>
#include<object.h>
#include<namespace.h>
#include<stdlib.h>
#include<mips.h>
#include<assert.h>

extern char __AES_start, __AES_end;

int
main() {
    for(int i=0; i<16; i++) {
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }

	void * u_ref = namespace_get_ref(5);
	assert(u_ref != NULL);
	void * u_id  = namespace_get_id(5);

    int64_t len = &__AES_end - &__AES_start;

    uint8_t *enc = (uint8_t *)malloc((size_t)len+32);
    uint8_t *encdec = (uint8_t *)malloc((size_t)len+32);

    const char *theKey = "0123456789ABCDEFFEDCBA98765432100123456789ABCDEFFEDCBA9876543210";

    int64_t encret = ccall_rrrr_r(u_ref, u_id, 0, (register_t)&__AES_start, (register_t)enc, len, (register_t)theKey);
    printf("Encryption length: %ld, ended with %ld status.\n", len, encret);

    uint64_t decret = ccall_rrrr_r(u_ref, u_id, 0, (register_t)enc, (register_t)encdec, -encret, (register_t)theKey);
    printf("Decryption ended with %ld status.\n", decret);

    printf("The first line of the decenc message:\n%s", encdec);

    return 0;
}
