#include "dsa.h"

#define BLEN (4096ull << 4)
#define DSA_OP_FLAG_US (1 << 16)
#define XFER_SIZE 4096

struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {};

int main(int argc, char *argv[]) {
    struct dsa_completion_record *probe_arr;
    probe_arr = (struct dsa_completion_record *) aligned_alloc(32, BLEN);
    memset(probe_arr, 0, BLEN);
    if (map_wq(&wq_info)) return EXIT_FAILURE;

    desc.opcode = DSA_OPCODE_COMPVAL;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.xfer_size = XFER_SIZE;
    desc.comp_pattern = 0;
    desc.expected_res = 0;
    desc.completion_addr = (uintptr_t) &comp;

    // Benchmarking ATC
    printf("%p\n", probe_arr);
    probe(probe_arr);
    probe(probe_arr + (4096 >> 6));

    return 0;
}

uint64_t probe(void *addr) {
    uint64_t start, end, retry = 0, ret = 0;
    comp.status = 0;
    desc.src_addr = (uintptr_t) addr;

resubmit:
    if (wq_info.wq_mapped)
        enqcmd(wq_info.wq_portal, &desc);
    else
        write(wq_info.wq_fd, &desc, sizeof(desc));

    start = rdtsc();
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&comp);
        if (comp.status == 0) {
            uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    end = rdtsc();
    ret += end - start;

    if (comp.status != DSA_COMP_SUCCESS) {
        printf("Failed: %d Bytes Completed: %d\n", comp.status,
               comp.bytes_completed);
        printf("Address: %p\n", (void *)comp.fault_addr);
        if (op_status(comp.status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            int wr = comp.status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp.fault_addr;
            wr ? *t = *t : *t;
            printf("retrying\n");
            goto resubmit;
        } else {
            printf("Error: %d\n", comp.status);
        }
    }

    return ret;
}
