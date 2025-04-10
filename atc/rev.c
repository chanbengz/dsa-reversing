#include "dsa.h"

#define BLEN (4096ull << 26)
#define DSA_OP_FLAG_US (1 << 16)

char *probe_arr;
struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {};


uint64_t probe(void* addr)
{
    uint64_t start, end;
    int retry = 0;
    desc.src_addr          = (uintptr_t) addr;
    memset(addr, 0, 8);

retry:
    enqcmd(wq_info.wq_portal, &desc);

    start = rdtsc();
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&(comp));
        if (comp.status == 0) {
        uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    end = rdtsc();

    if (comp.status != DSA_COMP_SUCCESS) {
        printf("Failed: %d Bytes Completed: %d\n", comp.status, comp.bytes_completed);
        printf("Address: %p\n", (void *)comp.fault_addr);
        if (op_status(comp.status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            int wr = comp.status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp.fault_addr;
            wr ? *t = *t : *t;
            goto retry;
        }
    }

    return end - start;
}

int main(int argc, char *argv[])
{
    uint64_t start, end;
    int retry = 0;

    probe_arr = malloc(BLEN);
    if (map_wq(&wq_info)) return EXIT_FAILURE;
    // warm up
    comp.status = 0;
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.completion_addr = (uintptr_t) &comp;
    enqcmd(wq_info.wq_portal, &desc);
    start = rdtsc();
    while (comp.status == 0) _mm_pause();
    end = rdtsc();
    printf("Warmup: %ld\n", end - start);

    printf("ATC: %ld\n", probe(probe_arr)); // Miss
    printf("ATC: %ld\n", probe(probe_arr)); // Hit
    for (int i = 12; i <= 20; i++) {
        printf("ATC: %ld\n", probe(probe_arr + (4096L << i)));
    }
    printf("ATC: %ld\n", probe(probe_arr)); // Miss
    return 0;
}
