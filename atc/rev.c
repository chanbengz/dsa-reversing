#include "dsa.h"
#include <stdint.h>
#include <sys/types.h>
#define TESTS_PER_PROFILE 1
#define BLEN (4096ull << 26) // 256GB
#define DSA_OP_FLAG_US (1 << 16)

struct dsa_completion_record* probe_arr;
struct dsa_completion_record arr_onbss = {};
struct wq_info wq_info;
struct dsa_hw_desc desc = {};
int probe_count = 0;


#define WARMUP_TESTS 16
#define TESTS_PER_PROBE 4
#define MAX_OFFSET 20
uint64_t results[MAX_OFFSET + 1][TESTS_PER_PROBE];

int main(int argc, char *argv[])
{
    struct dsa_completion_record arr_onstack __attribute__((aligned(32))) = {};
    probe_arr = (struct dsa_completion_record *)aligned_alloc(32, BLEN);
    memset(probe_arr, 0, BLEN >> 10);
    if (map_wq(&wq_info)) return EXIT_FAILURE;

    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;

    // Warm up
    void* base = probe_arr;
    uint64_t miss_t = 0, hit_t = 0;
    for (int i = 0; i < WARMUP_TESTS; i++) {
        probe(base); probe(&arr_onstack); probe(&arr_onbss);
        miss_t += probe(base);
        hit_t += probe(base);
    }

    // Benchmarking ATC
    printf("Cache miss: %ld\n", miss_t / WARMUP_TESTS);
    printf("Cache hit:  %ld\n", hit_t  / WARMUP_TESTS);

    uint64_t offset = 1L << 31;
    printf("base addr : %ld\n", probe(base)); // hit
    printf("offset    : %ld\n", probe(base + offset)); // hit
    printf("base addr : %ld\n", probe(base)); // hit

    // Different tests
    // printf("%p: %ld\n", &arr_onstack, probe(&arr_onstack));
    // printf("%p: %ld\n", base, probe(base));
    // printf("%p: %ld\n", &arr_onbss, probe(&arr_onbss));
    // printf("%p: %ld\n", &arr_onbss, probe(&arr_onbss));

    return 0;
}


uint64_t probe(void* addr)
{
    probe_count++;
    uint64_t start, end, retry = 0, ret = 0;
    struct dsa_completion_record* comp = (struct dsa_completion_record*) addr;
    desc.completion_addr = (uintptr_t) comp;
    memset(comp, 0, 8);

resubmit: 
    enqcmd(wq_info.wq_portal, &desc);
    start = rdtsc();
    while (comp->status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&(comp));
        if (comp->status == 0) {
        uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    end = rdtsc();
    ret += end - start;

    if (comp->status != DSA_COMP_SUCCESS) {
        printf("Failed: %d Bytes Completed: %d\n", comp->status, comp->bytes_completed);
        printf("Address: %p\n", (void *)comp->fault_addr);
        if (op_status(comp->status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            int wr = comp->status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp->fault_addr;
            wr ? *t = *t : *t;
            printf("retrying\n");
            goto resubmit;
        } else {
            printf("Error: %d\n", comp->status);
        }
    }
    return ret;
}