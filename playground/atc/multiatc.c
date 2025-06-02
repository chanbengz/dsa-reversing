#include "dsa.h"
#include <stdlib.h>
#include <string.h>

#define BLEN (4096ull << 10)
#define TESTS 100000

char *probe_arr, *probe_arr2;
struct wq_info wq_info;
struct dsa_hw_desc desc = {};

uint64_t probe_comp(void *addr1, void *addr2) {
    uint64_t start, end;
    int retry = 0;

    // Initialize the descriptor
    struct dsa_completion_record *comp = (struct dsa_completion_record *)addr2;
    comp->status = 0;
    desc.src_addr = (uintptr_t)addr1;
    desc.completion_addr = (uintptr_t)comp;
    memset(addr1, 0, 8);

retry:
    // enqcmd(wq_info.wq_portal, &desc);
    write(wq_info.wq_fd, &desc, sizeof(desc));

    start = rdtsc();
    while (comp->status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(comp);
        if (comp->status == 0) {
            uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    end = rdtsc();

    if (comp->status != DSA_COMP_SUCCESS) {
        printf("Failed: %d\n", comp->status);
        return -1;
    }

    return end - start;
}

int main(int argc, char *argv[]) {
    uint64_t start, end;
    int retry = 0;

    probe_arr = malloc(BLEN);
    probe_arr2 = aligned_alloc(32, BLEN);
    memset(probe_arr2, 0, 4096);

    if (map_wq(&wq_info)) return EXIT_FAILURE;
    // warm up
    desc.opcode = DSA_OPCODE_COMPVAL;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.comp_pattern = 0x0;
    desc.xfer_size = 8;
    desc.expected_res = 0;

    probe_comp(probe_arr, probe_arr2);

    uint64_t results[4] = {0};
    for (int i = 0; i < TESTS; i++) {
        probe_comp(probe_arr, probe_arr2);
        results[0] += probe_comp(probe_arr, probe_arr2);        // 2 hits
        results[1] += probe_comp(probe_arr + 4096, probe_arr2); // comp hit
        results[2] += probe_comp(probe_arr, probe_arr2 + 4096); // 2 misses
        results[3] += probe_comp(probe_arr, probe_arr2);        // src hit
    }

    printf("2 hits:   %4ld\n", results[0] / TESTS);
    printf("comp hit: %4ld\n", results[1] / TESTS);
    printf("2 misses: %4ld\n", results[2] / TESTS);
    printf("src hit:  %4ld\n", results[3] / TESTS);

    return 0;
}