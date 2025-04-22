#include "dsa.h"
#include <time.h>

#define BLEN (4096 << 0)  // 4KB
#define SUBMISSION_INTERVAL_MS 500

struct wq_info wq_info;

int submit_memmove(void* src, void* dst) {
    int rc;
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};
    
    // Prepare descriptor for memmove operation
    desc.opcode = DSA_OPCODE_MEMMOVE;
    desc.flags = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_CC;
    desc.xfer_size = BLEN;
    desc.src_addr = (uintptr_t)src;
    desc.dst_addr = (uintptr_t)dst;
    desc.completion_addr = (uintptr_t)&comp;

retry:
    // Submit descriptor
    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
            return EXIT_FAILURE;
    }
    
    // Wait for completion
    int retry = 0;
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&comp);
        if (comp.status == 0) {
            uint64_t delay = rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }

    // Handle completion status
    if (comp.status != DSA_COMP_SUCCESS) {
        if (op_status(comp.status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            // Handle page fault and continue
            int wr = comp.status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp.fault_addr;
            wr ? *t = *t : *t;
            desc.src_addr += comp.bytes_completed;
            desc.dst_addr += comp.bytes_completed;
            desc.xfer_size -= comp.bytes_completed;
            goto retry;
        } else {
            printf("[victim] Descriptor failed status %u\n", comp.status);
            rc = EXIT_FAILURE;
        }
    } else {
        rc = EXIT_SUCCESS;
    }

    return rc;
}

int main(int argc, char *argv[]) {
    printf("[victim] Starting vanilla victim, BLEN: %d\n", BLEN);
    
    // Allocate source and destination buffers
    char *src = malloc(BLEN);
    char *dst = malloc(BLEN);
    if (!src || !dst) {
        printf("[victim] Memory allocation failed\n");
        return EXIT_FAILURE;
    }
    
    // Initialize the source buffer with random data
    srand(time(NULL));
    memset(src, rand() % 256, BLEN);
    
    // Initialize work queue
    int rc = map_wq(&wq_info);
    if (rc) {
        printf("[victim] Failed to map work queue: %d\n", rc);
        free(src);
        free(dst);
        return EXIT_FAILURE;
    }
    
    printf("[victim] Submitting memmove descriptors every %d ms...\n", SUBMISSION_INTERVAL_MS);
    
    int submission_count = 0;
    
    // Submit memmove descriptors periodically
    while (1) {
        rc = submit_memmove(src, dst);
        submission_count++;
        
        printf("[victim] Submitted descriptor #%d\n", submission_count);
        
        if (rc != EXIT_SUCCESS) {
            printf("[victim] Memmove operation failed\n");
            break;
        }
        
        usleep(SUBMISSION_INTERVAL_MS * 1000);  // Convert ms to microseconds
    }
    
    return EXIT_SUCCESS;
}
