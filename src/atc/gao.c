#include "dsa.h"
#define TESTS_PER_PROFILE 1
#define BLEN (4096ull << 26) // 256GB
#define DSA_OP_FLAG_US (1 << 16)

struct dsa_completion_record* probe_arr;
struct wq_info wq_info;
struct dsa_hw_desc desc = {};

void profile(void* base, int w, int s)
{
    probe(base); // Miss
    // The base address is present in ATC
    for (int i = 0; i < TESTS_PER_PROFILE; i++) {
        for (int j = 1; j <= w + 1; j++) {
            void* addr = base + j * s * 4096;
            printf("Probing %p\n", addr);
            probe(addr);
        }
    }
    // Hit: not evicted
    // Miss: evicted
    probe(base);
}


#define TESTS_PER_PROBE 4
#define MAX_OFFSET 22
uint64_t results[MAX_OFFSET + 1][TESTS_PER_PROBE];

int main(int argc, char *argv[])
{
    probe_arr = (struct dsa_completion_record *)aligned_alloc(32, BLEN);
    memset(probe_arr, 0, BLEN >> 10);
    if (map_wq(&wq_info)) return EXIT_FAILURE;
    // Warm up
    probe_arr[0].status = 0;
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.completion_addr = (uintptr_t) &probe_arr[0];
    enqcmd(wq_info.wq_portal, &desc);
    while (probe_arr[0].status == 0) _mm_pause();

    // Benchmarking ATC
    void* base = probe_arr;
    switch (argc) {
        case 3:
            int w = atoi(argv[1]);
            int s = atoi(argv[2]);
            printf("Testing W = %d, S = %d\n", w, s);
            profile(base + (0x7E << 5), w, s);
            break;
       
        case 2:
            printf("Sleeping %s seconds\n", argv[1]);
            printf("start: %ld\n", probe(base));
            sleep(atoi(argv[1]));
            __builtin_ia32_lfence();
            printf("end:   %ld\n", probe(base));
            break;

        default:
            for (int j = 0; j < TESTS_PER_PROBE; j++) {
                probe(base);
                for (int i = 12; i <= MAX_OFFSET; i++) 
                    results[i][j] = probe(base + (4096ull << i));
                probe(base);
            }

            // ----- header
            for (int i = 12; i <= MAX_OFFSET; i++) printf("| %4d ", i); printf("|\n");
            for (int i = 12; i <= MAX_OFFSET; i++) printf("|------");   printf("|\n");
            // ----- body
            for (int j = 0; j < TESTS_PER_PROBE; j++) {
                for (int i = 12; i <= MAX_OFFSET; i++) printf("| %4ld ", results[i][j]);
                printf("|\n");
            }
            break;
    }

    return 0;
}


uint64_t probe(void* addr)
{
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