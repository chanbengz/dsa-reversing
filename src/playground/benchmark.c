#include "dsa.h"
#include <linux/idxd.h>
#include <stdlib.h>


#define BLEN (4096 << 0)
#define TEST_NUM 2

uint64_t start;
uint64_t submit = 0;
uint64_t wait = 0;

int main(int argc, char *argv[])
{
    char* src = malloc(BLEN * TEST_NUM * sizeof(char));
    char* dst = malloc(BLEN * TEST_NUM * sizeof(char));
    for (int i = 0; i < TEST_NUM; i++) memset(src + i * BLEN, 0x41 + i, BLEN);

    // skip the first, since it results in TLB miss
    submit_wd(src, dst);
    submit = wait = 0;
    for (int i = 1; i < TEST_NUM; i++) {
        int result = submit_wd(src + (BLEN << 1), dst + i * BLEN);
        if (result) return 1;
    }

    printf("BLEN: %d\n", BLEN);

    return 0;
}

inline int submit_wd(void* src, void *dst) {
    int retry;
    struct dsa_hw_desc* desc = aligned_alloc(64, sizeof(struct dsa_hw_desc));
    struct dsa
    comp.status          = 0;
    desc->opcode          = DSA_OPCODE_MEMMOVE;
    desc->flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc->xfer_size       = BLEN;
    desc->src_addr        = (uintptr_t)src;
    desc->dst_addr        = (uintptr_t)dst;
    desc->completion_addr = (uintptr_t)comp;
}
