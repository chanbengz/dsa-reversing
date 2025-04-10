#include "dsa.h"

#define TEST_NUM 10

uint64_t BLEN = (4096ull << 0);
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
   
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};
    comp.status = 0;
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.completion_addr = (uintptr_t) &comp;
    enqcmd(wq_info.wq_portal, &desc);

    while(comp.status == 0);

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
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};

    desc.opcode          = DSA_OPCODE_MEMMOVE;
    desc.flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc.xfer_size       = BLEN;
    desc.src_addr        = (uintptr_t) src;
    desc.dst_addr        = (uintptr_t) dst;
    desc.completion_addr = (uintptr_t) &comp;

submit:
    memset(&comp, 0, sizeof(comp));
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

    if (comp.status != DSA_COMP_SUCCESS) {
        if (op_status(comp.status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            // continue to finish the rest
            int wr = comp.status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp.fault_addr;
            wr ? *t = *t : *t;
            desc.src_addr += comp.bytes_completed;
            desc.dst_addr += comp.bytes_completed;
            desc.xfer_size -= comp.bytes_completed;
            goto submit;
        } else {
            printf("completion status: 0x%x\n", comp.status);
        }
    } else {
        int rc = memcmp(src, dst, BLEN);
        rc ? printf("memmove failed: %d\n", rc) \
            : printf("memmove successful\n");
    }

    return 0;
}
