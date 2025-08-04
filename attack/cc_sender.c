#include "dsa.h"

#define XFER_SIZE (4096 << 6)

struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record* comp_array;

static inline void send_bit(struct dsa_completion_record* comp) {
    memset(comp, 0, sizeof(struct dsa_completion_record));
    desc.completion_addr = (uintptr_t) comp;
    enqcmd(wq_info.wq_portal, &desc);
}

void send_char(char c) {
    for (int i = 0; i < 8; i++) {
        int bit = (c >> i) & 1;
        for(int i = 0; i < BITS_REPEAT; i++) {
            uint64_t start = rdtsc();
            if (bit) {
                send_bit(comp_array);
            }
            while (rdtsc() - start < BIT_INTERVAL_NS);
        }
    }
}

int main(int argc, char *argv[]) {
    comp_array = (struct dsa_completion_record*) \
        aligned_alloc(32, XFER_SIZE);
    if (!comp_array) {
        fprintf(stderr, "[cc_sender] Failed to allocate memory\n");
        return EXIT_FAILURE;
    }
    memset(comp_array, 0, XFER_SIZE);

    int rc = map_spec_wq(&wq_info, "/dev/dsa/wq0.0");
    if (rc) {
        fprintf(stderr, "[cc_sender] Failed to map work queue: %d\n", rc);
        free(comp_array);
        return EXIT_FAILURE;
    }
    
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    
    printf("[cc_sender] Ready to transmit data...\n");
    
    char message[CHARS_TO_RECEIVE];
    FILE *fp = fopen("msg", "rb");
    if (!fp) {
        perror("[cc_sender] Failed to open message file");
        return EXIT_FAILURE;
    }

    size_t message_len = fread(message, 1, CHARS_TO_RECEIVE, fp);
    fclose(fp);
    if (message_len == 0) {
        perror("[cc_sender] Failed to read message file");
        return EXIT_FAILURE;
    }

    size_t message_pos = 0;
    // printf("[cc_sender] Sending %lx\n", *(uint64_t*) message);

    while (message_len--) {
        // scanf("%*c"); // Wait for user input to send next char
        for (int i = 0; i < START_BITS; i++) {
            send_bit(comp_array);
        }

        _mm_mfence();
        nsleep(10000); // receiver needs to record time
        send_char(message[message_pos++]);
        usleep(10); // Give some time for the receiver to process
    }
    
    return EXIT_SUCCESS;
} 