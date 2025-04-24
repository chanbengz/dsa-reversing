#include "dsa.h"
#include <unistd.h>

#define BIT_INTERVAL_US 100000
#define UART_START_BITS 2    // Two consecutive 1s for start
#define UART_STOP_BIT 1      // Stop bit is always 1
#define DATA_BITS 8          // 8 data bits

struct wq_info wq_info;
struct dsa_hw_desc desc = {};

int submit_noop(struct dsa_completion_record* comp) {
    memset(comp, 0, sizeof(struct dsa_completion_record));
    desc.completion_addr = (uintptr_t)comp;

    // Submit descriptor
    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
            return EXIT_FAILURE;
    }
    
    int retry = 0;
    while (comp->status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(comp);
        if (comp->status == 0) {
            uint64_t delay = rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }

    if (comp->status != DSA_COMP_SUCCESS) {
        printf("[cc_sender] Descriptor failed with status %u\n", comp->status);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

void send_bit(struct dsa_completion_record* comp_array, int bit) {
    bit ? submit_noop(comp_array) : usleep(1);
    usleep(BIT_INTERVAL_US);
}

void send_uart_char(struct dsa_completion_record* comp_array, char c) {
    for (int i = 0; i < UART_START_BITS; i++) {
        send_bit(comp_array, 1);
    }
    
    for (int bit = 0; bit < DATA_BITS; bit++) {
        int bit_value = (c >> bit) & 1;
        send_bit(comp_array, bit_value);
    }
    
    send_bit(comp_array, UART_STOP_BIT);
}

int main(int argc, char *argv[]) {
    printf("[cc_sender] Starting modified UART-style covert channel sender\n");
    printf("[cc_sender] Using %d consecutive 1s as start sequence\n", UART_START_BITS);
    
    size_t comp_array_size = 4096 * 8;
    struct dsa_completion_record* comp_array = 
        (struct dsa_completion_record*)aligned_alloc(32, comp_array_size);
    if (!comp_array) {
        fprintf(stderr, "[cc_sender] Failed to allocate memory\n");
        return EXIT_FAILURE;
    }
    memset(comp_array, 0, comp_array_size);
    
    int rc = map_wq(&wq_info);
    if (rc) {
        fprintf(stderr, "[cc_sender] Failed to map work queue: %d\n", rc);
        free(comp_array);
        return EXIT_FAILURE;
    }
    
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    
    printf("[cc_sender] Ready to transmit data...\n");
    
    const char* message = "HELLO";
    size_t message_len = strlen(message);
    size_t message_pos = 0;
    
    while (1) {
        if (message_pos >= message_len) {
            message_pos = 0;
            usleep(BIT_INTERVAL_US * 10);
        }
        
        char current_char = message[message_pos++];
        send_uart_char(comp_array, current_char);
        
        usleep(BIT_INTERVAL_US * 2);
    }
    
    return EXIT_SUCCESS;
} 