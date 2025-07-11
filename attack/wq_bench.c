#include "dsa.h"

#define XFER_SIZE (1L << 26)
#define WQ_SIZE 7
const int TIMESTAMP_ENABLED = 1;

struct wq_info wq_info;
struct dsa_hw_desc noop = {}, mass[WQ_SIZE] = {}, noops[WQ_SIZE] = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {}, \
    mass_comp[WQ_SIZE] __attribute__((aligned(32))) = {}, \
    noops_comp[WQ_SIZE] __attribute__((aligned(32))) = {};
uint8_t *src, *dst;
double time_elapsed = 0.0;

static inline void congest() {
    mass_comp[0].status = 0;
    while (enqcmd(wq_info.wq_portal, &mass[0]));
    for(int i = 1; i < WQ_SIZE - 1; i++) {
        noops_comp[i].status = 0;
        if (enqcmd(wq_info.wq_portal, &noops[i]))
            printf("failed to enqueue noop %d\n", i);
    }
}

static inline int probe_swq() {
    return enqcmd(wq_info.wq_portal, &noop);
}

static inline int receive() {
    congest();
    nsleep(WQ_INTERVAL_NS);
    return probe_swq();
}

int main(int argc, char *argv[]) {
    // Initialize work queue
    int rc = map_spec_wq(&wq_info, "/dev/dsa/wq2.0");
    if (rc) {
        fprintf(stderr, "[wq_spy] Failed to map work queue: %d\n", rc);
        return EXIT_FAILURE;
    }
    
    src = malloc(XFER_SIZE * sizeof(uint8_t));
    dst = malloc(XFER_SIZE * sizeof(uint8_t));
    memset(src, 0, XFER_SIZE * sizeof(uint8_t)); // no page fault
    memset(dst, 0, XFER_SIZE * sizeof(uint8_t));

    noop.opcode = DSA_OPCODE_NOOP;
    noop.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    noop.completion_addr = (uintptr_t) &comp;
    for (int i = 0; i < WQ_SIZE; i++) {
        mass[i].opcode = DSA_OPCODE_MEMMOVE;
        mass[i].flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CC;
        mass[i].src_addr = (uintptr_t) src;
        mass[i].dst_addr = (uintptr_t) dst;
        mass[i].xfer_size = XFER_SIZE;
        mass_comp[i].status = 0;
        mass[i].completion_addr = (uintptr_t) &mass_comp[i];

        noops[i].opcode = DSA_OPCODE_NOOP;
        noops[i].flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
        noops[i].completion_addr = (uintptr_t) &noops_comp[i];
    }
    
    for (int i = 15; i < 28; i++) {
        mass[0].xfer_size = (1L << i);
        int low = 0, high = INT32_MAX;

        while (low < high) {
            int mid = (low + high) / 2;
            congest();
            if (probe_swq()) printf("victim cannot submit\n");
            nsleep(mid);
            // printf("swq: %s\n", probe_swq() ? "busy" : "idle");
            int rc = probe_swq();
            if (rc) low = mid + 1;
            else high = mid - 1;
            usleep(1);
        }

        printf("Xfer size 2^%d Interval: %d\n", i, low);
    }

    return EXIT_SUCCESS;
}
