#include "dsa.h"
#include <threads.h>

struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record* comp_array;

static inline void send_bit(struct dsa_completion_record* comp) {
    memset(comp, 0, sizeof(struct dsa_completion_record));
    // desc.src_addr = (uintptr_t) comp + 32;
    desc.completion_addr = (uintptr_t) comp;

    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
            return;
    }
}

void send_char(char c) {
    for (int i = 0; i < 8; i++) {
        int bit = (c >> i) & 1;
        for(int i = 0; i < BITS_REPEAT; i++) {
            if (bit) {
                uint64_t start = rdtsc();
                send_bit(comp_array);
                while (rdtsc() - start < BIT_INTERVAL_NS)
                    _mm_pause();
            } else {
                nsleep(BIT_INTERVAL_NS);
            }
        }
    }
}

int map_another_wq(struct wq_info* wq_info) {
    wq_info->wq_mapped = false;
    wq_info->wq_fd = open("/dev/dsa/wq0.1", O_RDWR);
    if (wq_info->wq_fd < 0) {
        return -errno;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    size_t comp_array_size = 4096 * 64;
    comp_array = (struct dsa_completion_record*)aligned_alloc(32, comp_array_size);
    if (!comp_array) {
        fprintf(stderr, "[cc_sender] Failed to allocate memory\n");
        return EXIT_FAILURE;
    }
    memset(comp_array, 0, comp_array_size);
    
    int rc = map_another_wq(&wq_info);
    if (rc) {
        fprintf(stderr, "[cc_sender] Failed to map work queue: %d\n", rc);
        free(comp_array);
        return EXIT_FAILURE;
    }
    
    // desc.opcode = DSA_OPCODE_COMPARE;
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    // desc.pattern = 0x0;
    // desc.xfer_size = 8;
    // desc.expected_res = 0;
    
    printf("[cc_sender] Ready to transmit data...\n");
    
    const char* message = "HELLO";
    size_t message_len = strlen(message);
    size_t message_pos = 1;
    
    int repeat = 1;
    for (int i = 0; i < 8; i++) {
        printf("[cc_sender] Sending %d\n", (message[message_pos] >> i) & 1);
    }
    while (repeat--) {
        for (int i = 0; i < START_BITS; i++) {
            send_bit(comp_array);
        }

        _mm_mfence();
        send_char(message[message_pos]);
    }
    printf("[cc_sender] Done transmitting data\n");
    
    return EXIT_SUCCESS;
} 