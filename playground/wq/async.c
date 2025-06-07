#include "dsa.h"
#include <stdint.h>

#define TEST_NUM 18

uint64_t BLEN = (4096ull << 0);
uint64_t start;
uint64_t submit = 0;
struct wq_info wq_info;
uint64_t submit_buf[TEST_NUM + 1] = {0}, occupy_buf[TEST_NUM + 1] = {0};
char *src, *dst;

int submit_async() {
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};

    desc.completion_addr = (uintptr_t) &comp;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.opcode = DSA_OPCODE_MEMMOVE;
    desc.src_addr = (uintptr_t) src;
    desc.dst_addr = (uintptr_t) dst;

    for (int i = 0; i < TEST_NUM; i++) {
        desc.xfer_size = (4096 << i);
        start = rdtsc();
        _mm_sfence();
        if (enqcmd(wq_info.wq_portal, &desc)) occupy_buf[i] = 1;
        submit_buf[i] = rdtsc() - start;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    src = malloc((BLEN << TEST_NUM) * sizeof(char));
    dst = malloc((BLEN << TEST_NUM) * sizeof(char));
    memset(src, 0xCB, BLEN << TEST_NUM);
    memset(dst, 0x00, BLEN << TEST_NUM);

    if (map_wq(&wq_info)) return EXIT_FAILURE;

    // skip the first, since it results in TLB miss
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};
    comp.status = 0;
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.completion_addr = (uintptr_t) &comp;
    enqcmd(wq_info.wq_portal, &desc);

    FILE *async_file = fopen("async.txt", "w");
    while (comp.status == 0);
    
    submit_async();
    
    for (int i = 0; i < TEST_NUM; i++) {
        fprintf(async_file, "%ld\n", submit_buf[i]);
        printf("submit %2d: %ld cycles, occupy: %ld\n", i, submit_buf[i], occupy_buf[i]);
    }

    fclose(async_file);

    return 0;
}
