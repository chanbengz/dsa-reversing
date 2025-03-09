#include "dsa.h"
#include <stdint.h>

#define TEST_NUM 5

uint64_t BLEN = (4096 << 0);
uint64_t start;
uint64_t submit = 0;
uint64_t wait = 0;
struct wq_info wq_info;

int main(int argc, char *argv[])
{
    char* src = malloc((BLEN << TEST_NUM) * sizeof(char));
    char* dst = malloc((BLEN << TEST_NUM) * sizeof(char));
    memset(src, 0xCB, BLEN << TEST_NUM);

    if (map_wq(&wq_info)) return EXIT_FAILURE;

    // skip the first, since it results in TLB miss
    // fill up IOTLB
    BLEN = (4096 << TEST_NUM);
    if (submit_wd(src, dst)) return 1;
    printf("[benchmark] warm up done\n");

    for (int i = 0; i < TEST_NUM; i++) {
        BLEN = (4096 << i);
        submit = wait = 0;
        if (submit_wd(src, dst)) return 1;
        printf("------------------------------------\n");
        printf("[benchmark] BLEN:       %6ld bytes\n", BLEN);
        printf("[benchmark] submission: %6ld cycles\n", submit);
        printf("[benchmark] wait:       %6ld cycles\n", wait);
    }

    return 0;
}

inline int submit_wd(void* src, void *dst) {
    int retry = 0;
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp = {};

    comp.status          = 0;
    desc.opcode          = DSA_OPCODE_MEMMOVE;
    desc.flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc.xfer_size       = BLEN;
    desc.src_addr        = (uintptr_t) src;
    desc.dst_addr        = (uintptr_t) dst;
    desc.completion_addr = (uintptr_t) &comp;

    // submit
    start = rdtsc();
    _mm_sfence();
    enqcmd(wq_info.wq_portal, &desc);
    submit += rdtsc() - start;

    start = rdtsc();
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&(comp));
        if (comp.status == 0) {
        uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    wait += rdtsc() - start; 

    return 0;
}
