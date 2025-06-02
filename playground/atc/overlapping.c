#include "dsa.h"
#include <linux/idxd.h>
#include <stdint.h>

#define BLEN (4096ull << 2)
#define TESTS 100000

char *probe_arr, *probe_arr2;
struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {};

uint64_t probe_memcpy(void *src, void *dst) {
    uint64_t start, end; int retry = 0;

    // Initialize the descriptor
    desc.opcode = DSA_OPCODE_MEMMOVE;

    comp.status = 0;
    desc.src_addr = (uintptr_t) src;
    desc.dst_addr = (uintptr_t) dst;
    desc.xfer_size = 8;
    desc.completion_addr = (uintptr_t) &comp;

    memset(src, 0x41, 8);

    if (wq_info.wq_mapped) { enqcmd(wq_info.wq_portal, &desc); }
    else { write(wq_info.wq_fd, &desc, sizeof(desc)); }

    start = rdtsc();
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&comp);
        if (comp.status == 0) {
            uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    end = rdtsc();

    if (comp.status != DSA_COMP_SUCCESS) {
        printf("Failed: %d\n", comp.status);
        return -1;
    }

    return end - start;
}

uint64_t probe_memcmp(void *src, void *dst) {
    uint64_t start, end; int retry = 0;

    // Initialize the descriptor
    desc.opcode = DSA_OPCODE_COMPARE;

    comp.status = 0;
    desc.src_addr = (uintptr_t) src;
    desc.src2_addr = (uintptr_t) dst;
    desc.xfer_size = 8;
    desc.expected_res = 0;
    desc.completion_addr = (uintptr_t) &comp;

    memset(src, 0x41, 8);

    if (wq_info.wq_mapped) { enqcmd(wq_info.wq_portal, &desc); }
    else { write(wq_info.wq_fd, &desc, sizeof(desc)); }

    start = rdtsc();
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&comp);
        if (comp.status == 0) {
            uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    end = rdtsc();

    if (comp.status != DSA_COMP_SUCCESS) {
        printf("Failed: %d\n", comp.status);
        return -1;
    }

    return end - start;
}

int main(int argc, char *argv[]) {
    probe_arr = malloc(BLEN);
    probe_arr2 = aligned_alloc(32, BLEN);
    memset(probe_arr, 0, BLEN);
    memset(probe_arr2, 0, BLEN);

    if (map_wq(&wq_info)) return EXIT_FAILURE;
    // warm up
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;

    probe_memcpy(probe_arr, probe_arr2);
    probe_memcmp(probe_arr, probe_arr2);
    probe_memcpy(probe_arr, probe_arr2);
    probe_memcmp(probe_arr, probe_arr2);

    return 0;
}