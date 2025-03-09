#include "dsa.h"

#define BLEN (4096 << 0)
#define TEST_NUM 2

uint64_t start;
uint64_t submit = 0;
uint64_t wait = 0;
struct wq_info wq_info;

int main(int argc, char *argv[])
{
    char* src = malloc(BLEN * TEST_NUM * sizeof(char));
    char* dst = malloc(BLEN * TEST_NUM * sizeof(char));
    for (int i = 0; i < TEST_NUM; i++) memset(src + i * BLEN, 0x41 + i, BLEN);
    printf("BLEN: %d\n", BLEN);

    if (map_wq(&wq_info)) return EXIT_FAILURE;

    // skip the first, since it results in TLB miss
    submit_wd(src, dst);
    submit = wait = 0;
    for (int i = 1; i < TEST_NUM; i++)
        if (submit_wd(src + i * BLEN, dst + i * BLEN)) return 1;

    printf("[benchmark] submission: %ld\n", submit);
    printf("[benchmark] wait: %ld\n", wait);

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
