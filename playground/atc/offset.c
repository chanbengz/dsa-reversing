#include "dsa.h"
#define TESTS_PER_PROFILE 1
#define BLEN (4096ull << 26) // 256GB
#define DSA_OP_FLAG_US (1 << 16)

struct dsa_completion_record *probe_arr;
struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record comp_onbss __attribute__((aligned(32))) = {};
int probe_count = 0;

#define TESTS_PER_PROBE 8
#define WARMUP_TESTS 10000
#define START_OFFSET 24
#define MAX_OFFSET 37
uint64_t results[MAX_OFFSET + 1][TESTS_PER_PROBE + 1];

int main(int argc, char *argv[]) {
    if (argc > 3) {
        printf("Usage: %s [sleep_time]\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct dsa_completion_record comp_onstack __attribute__((aligned(32))) = {};
    probe_arr = (struct dsa_completion_record *)aligned_alloc(32, BLEN);
    memset(probe_arr, 0, BLEN >> 10);
    if (map_wq(&wq_info)) return EXIT_FAILURE;
    // Warm up
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;

    // Benchmarking ATC
    void *base = probe_arr;
    uint64_t miss_t = 0, hit_t = 0;
    for (int i = 0; i < WARMUP_TESTS; i++) {
        // eviction
        probe(base);
        probe(&comp_onbss);
        probe(&comp_onstack);
        miss_t += probe(base);
        hit_t += probe(base);
    }
    printf("Cache miss: %ld\n", miss_t / WARMUP_TESTS);
    printf("Cache hit:  %ld\n", hit_t / WARMUP_TESTS);

    // Different tests
    for (int j = 0; j < TESTS_PER_PROBE; j++) {
        for (int i = START_OFFSET; i <= MAX_OFFSET; i++) {
            probe(base);
            results[i][j] = probe(base + (1L << i));
            probe(base);
        }
    }

    // print results
    // ----- header
    for (int i = START_OFFSET; i <= MAX_OFFSET; i++) printf("| %4d ", i);
    printf("|\n");
    for (int i = START_OFFSET; i <= MAX_OFFSET; i++) printf("|------");
    printf("|\n");
    // ----- body
    for (int j = 0; j < TESTS_PER_PROBE; j++) {
        for (int i = START_OFFSET; i <= MAX_OFFSET; i++)
            printf("| %4ld ", results[i][j]);
        printf("|\n");
    }
    printf("probe_count: %d\n", probe_count);

    return 0;
}

uint64_t probe(void *addr) {
    probe_count++;
    uint64_t start, end, retry = 0, ret = 0;
    struct dsa_completion_record *comp = (struct dsa_completion_record *)addr;
    desc.completion_addr = (uintptr_t)comp;
    memset(comp, 0, 8);

resubmit:
    // enqcmd(wq_info.wq_portal, &desc);
    write(wq_info.wq_fd, &desc, sizeof(desc));
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
        printf("Failed: %d Bytes Completed: %d\n", comp->status,
               comp->bytes_completed);
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