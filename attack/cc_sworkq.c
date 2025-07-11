#include "dsa.h"

struct wq_info wq_info;
struct dsa_hw_desc desc;
struct dsa_completion_record comp __attribute__((aligned(32))) = {};

static inline void send_bit() { enqcmd(wq_info.wq_portal, &desc); }

void send_char(char c) {
    for (int i = 0; i < 8; i++) {
        int bit = (c >> i) & 1;
        for(int i = 0; i < BITS_REPEAT; i++) {
            uint64_t start = rdtsc();
            if (bit) {
                send_bit();
            }
            while (rdtsc() - start < WQ_INTERVAL_NS);
        }
    }
}

int main() {
    // Initialize work queue
    int rc = map_spec_wq(&wq_info, "/dev/dsa/wq2.0");
    if (rc) {
        fprintf(stderr, "[wq_spy] Failed to map work queue: %d\n", rc);
        return EXIT_FAILURE;
    }

    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.completion_addr = (uintptr_t) &comp;

    char message[CHARS_TO_RECEIVE];
    FILE *fp = fopen("msg", "rb");
    if (!fp) {
        fprintf(stderr, "[cc_sender] Failed to open message file\n");
        return EXIT_FAILURE;
    }
    size_t message_len = fread(message, 1, CHARS_TO_RECEIVE, fp);
    fclose(fp);

    printf("[cc_sender] Ready to send: %c\n", message[0]);

    for (int i = 0; i < message_len; i++) {
        // scanf("%*c"); // Wait for user input to send next char
        for (int j = 0; j < START_BITS; j++) {
            send_bit();
            nsleep(WQ_INTERVAL_NS - 10000);
        }

        _mm_mfence();
        nsleep(10000); // receiver needs to record time
        send_char(message[i]);
        nsleep(100000); // Give some time for the receiver to process
    }

    return 0;
}