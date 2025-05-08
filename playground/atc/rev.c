#include "dsa.h"
#define BLEN (4096ull << 26) // 256GB
#define DSA_OP_FLAG_US (1 << 16)

struct dsa_completion_record* probe_arr;
struct dsa_completion_record arr_onbss[4096] __attribute__((aligned(32)));
struct wq_info wq_info;
struct dsa_hw_desc desc = {};
int probe_count = 0;


#define WARMUP_TESTS 2
#define TESTS_PER_PROBE 10000
#define MAX_OFFSET 33
#define EVCTION_SETS 2

int main(int argc, char *argv[])
{
    struct dsa_completion_record arr_onstack[4] __attribute__((aligned(32))) = {0,0};
    probe_arr = (struct dsa_completion_record *)aligned_alloc(32, BLEN);
    memset(probe_arr, 0, BLEN >> 10);
    if (map_wq(&wq_info)) return EXIT_FAILURE;

    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;

    // Warm up
    void* base = probe_arr;
    for (int i = 0; i < WARMUP_TESTS; i++) {
        probe(base);
        probe(arr_onstack); 
        probe(arr_onbss);
    }

    if (argc != 2) {
        printf("Usage: %s <0: offset, 1: addr, 2: evict>\n", argv[0]);
        return 1;
    }

    uint64_t offset = 1L << MAX_OFFSET;
    uint64_t first_t = 0, second_t = 0, third_t = 0;
    uint64_t results[5] = {0, 0, 0, 0};
    uint64_t result = 0;

    switch (atoi(argv[1])) {
        // Experiment 1: Latency introduced by eviction
        case 0:
            for (int i = 0; i < TESTS_PER_PROBE; i++) {
                first_t += probe(base); // hit
                second_t += probe(base + offset); // miss
                third_t += probe(base); // miss
            }
            printf("base addr : %ld\n", first_t / TESTS_PER_PROBE); 
            printf("offset    : %ld\n", second_t / TESTS_PER_PROBE);
            printf("base addr : %ld\n", third_t / TESTS_PER_PROBE);
            break;
        // Experiment 2: Latency in different locations (and eviction)
        case 1: 
            for (int i = 0; i < TESTS_PER_PROBE; i++) {
                results[0] += probe(arr_onstack);
                results[1] += probe(base);
                results[2] += probe(arr_onbss);
                results[3] += probe(base);
            }
            printf("%p: %ld\n", arr_onstack, results[0] / TESTS_PER_PROBE);
            printf("%p: %ld\n", base, results[1] / TESTS_PER_PROBE);
            printf("%p: %ld\n", &arr_onbss, results[2] / TESTS_PER_PROBE);
            printf("%p: %ld\n", base, results[3] / TESTS_PER_PROBE);
            break;
        // Experiment 3: Trial of second level DevTLB
        case 2:
            for (int i = 0; i < TESTS_PER_PROBE; i++) {
                for (int j = 0; j < EVCTION_SETS; j++) probe(base + (4096L << j));
                result += probe(base);
            }
            printf("evict trial: %ld\n", result / TESTS_PER_PROBE);
            break;
        default:
            printf("Invalid argument\n");
            break;
    }

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
