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
            if (bit) { send_bit(); }
            while (rdtsc() - start < WQ_INTERVAL_NS);
        }
    }
}

int main() {
    // Initialize work queue
    int rc = map_spec_wq(&wq_info, "/dev/dsa/wq0.0");
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

restart:
    for (int i = 0; i < CHARS_TO_RECEIVE; i++) {
        for (int j = 0; j < START_BITS; j++) {
            comp.status = 0;
            if (enqcmd(wq_info.wq_portal, &desc)) {
                printf("failed to enqueue sync noop at char %d\n", i);
                nsleep(10000);
                goto restart;
            }
            nsleep(WQ_INTERVAL_NS);
        }

        while (comp.status == 0) _mm_pause();
        nsleep(40000);
        send_char(message[i]);
        nsleep(30000);
    }

    return 0;
}