#include<stdio.h>
#include<object.h>
#include<namespace.h>
#include<stdlib.h>
#include<mips.h>
#include<assert.h>
#include<sha_info.h>
#include<statcounters.h>

#define EACH_BLOCK_SIZE 256

extern char __AES_start, __AES_end;

int
main() {
    stats_init();
    for(int i=0; i<8; i++) {
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }

	void * u_ref = namespace_get_ref(5);
	assert(u_ref != NULL);
	void * u_id  = namespace_get_id(5);

    // get the direct jump address of AES
	void * u_entry = namespace_get_entry(5);
	assert(u_entry != NULL);
	void * u_base  = namespace_get_base(5);
	assert(u_base != NULL);

	void * sha_ref = namespace_get_ref(6);
	assert(sha_ref != NULL);
	void * sha_id  = namespace_get_id(6);

    size_t len = &__AES_end - &__AES_start;
    size_t encdecOffset = 0, totalDeced = 0, remain = 0;
    int64_t encret;
    uint64_t decret;

    uint8_t *enc = (uint8_t *)malloc(EACH_BLOCK_SIZE + 32);
    uint8_t *encdec = (uint8_t *)malloc((size_t)len + 32);

    const char *theKey = "0123456789ABCDEFFEDCBA98765432100123456789ABCDEFFEDCBA9876543210";

    while((remain = len-encdecOffset) > EACH_BLOCK_SIZE) {
        encret = dcall_4(0, ((size_t)&__AES_start + encdecOffset), (register_t)enc, EACH_BLOCK_SIZE, (register_t)theKey, u_entry, u_base);
        decret = dcall_4(0, (register_t)enc, (size_t)encdec + totalDeced, -encret, (register_t)theKey, u_entry, u_base);
        encdecOffset += EACH_BLOCK_SIZE;
        totalDeced += decret;
    }
    encret = dcall_4(0, ((size_t)&__AES_start + encdecOffset), (register_t)enc, remain, (register_t)theKey, u_entry, u_base);
    decret = dcall_4(0, (register_t)enc, (size_t)encdec + totalDeced, -encret, (register_t)theKey, u_entry, u_base);
    totalDeced += decret;

    printf("Size of the original: %ld, Total bytes decrypted: %ld\n", len, totalDeced);

    SHA_INFO theinfo;
    ccall_4(sha_ref, sha_id, 0, (register_t)&theinfo, (register_t)&__AES_start, (size_t)len, 0);
    ccall_4(sha_ref, sha_id, 1, (register_t)&theinfo, 0, 0, 0);

    stats_display();
    return 0;
}
