#include "dsa.h"
#include "cc.h"

#define XFER_SIZE (1 << 28)
#define WQ_SIZE 7
#define TRIAL 4

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
    nsleep(WQ_INTERVAL_NS - 20000);
    return probe_swq();
}

int main(int argc, char *argv[]) {
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
        mass[i].completion_addr = (uintptr_t) &mass_comp[i];
        noops[i].opcode = DSA_OPCODE_NOOP;
        noops[i].flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
        noops[i].completion_addr = (uintptr_t) &noops_comp[i];
    }

    int low = 0, high = XFER_SIZE;
    while (low <= high) {
        int mid = (low + high) / 2;
        mass[0].xfer_size = mid;
        
        int tmp = 0;
        for (int i = 0; i < TRIAL; i++) {
            congest();
            nsleep(1000);
            comp.status = 0;
            if (probe_swq()) printf("victim cannot submit initially\n");
            nsleep(WQ_INTERVAL_NS - 1000);
            if (probe_swq()) tmp++;
            else break;
            while (comp.status == 0);
        }

        if (tmp == TRIAL) high = mid - 1;
        else low = mid + 1;
    }
    mass[0].xfer_size = low;

    struct timespec ts;
    char msg[CHARS_TO_RECEIVE];
    printf("[cc_receiver] Ready to receive bits...\n");
    for (int j = 0; j < CHARS_TO_RECEIVE; j++) {
        uint8_t received_char = 0, bits, consecutive_bits = 0;

        while (1) {
            consecutive_bits = (receive()) ? consecutive_bits + 1 : 0;
            if (consecutive_bits >= START_BITS) break;
        }

        // sync
        while (noops_comp[WQ_SIZE - 3].status == 0) _mm_pause();
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double start_time = (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;

        for (int i = 0; i < BITS_TO_RECEIVE; i++) {
            bits = 0;
            for (int i = 0; i < BITS_REPEAT; i++) {
                bits += receive();
            }
            received_char |= (bits > (BITS_REPEAT / 3)) ? (1 << i) : 0;
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        time_elapsed += ((double) ts.tv_sec + (double) ts.tv_nsec / 1e9) - start_time;
        msg[j] = received_char;
    }

    printf("[cc_receiver] Time elapsed: %.6f seconds\n", time_elapsed);
    FILE *fp = fopen("recv", "wb");
    if (!fp) {
        fprintf(stderr, "[cc_receiver] Failed to open output file\n");
        return EXIT_FAILURE;
    }
    fwrite(msg, 1, CHARS_TO_RECEIVE, fp);
    fclose(fp);

    return EXIT_SUCCESS;
}
