#include "dsa.h"
#include <stdio.h>

#define BLEN (4096 << 0)
#define TEST_NUM 8

uint64_t fails = 0;
struct wq_info wq_info;

int main(int argc, char *argv[])
{
    printf("[victim] starts, BLEN: %d\n", BLEN);
    char *src = malloc(BLEN), *dst = malloc(BLEN);
    srand(114514LL);

    int rc = map_wq(&wq_info);
    if (rc) return EXIT_FAILURE;

    for (int i = 0; i < TEST_NUM; i++) {
        fails = 0; memset(src, rand(), BLEN);
        int result = submit_wd(src, dst);
        fails ? printf("[victim] Submission failures: %ld\n", fails) \
              : printf("[victim] Submission successful\n");
    }

    return 0;
}

static inline void submit_desc_check(void *wq_portal, struct dsa_hw_desc *hw)
{
    while (enqcmd(wq_portal, hw)) fails++; 
}

int submit_wd(void* src, void *dst) {
    /*printf("src: %p, dst: %p, char: %c\n", src, dst, *(char *)src);*/
    int rc;
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};
    
    // prepare descriptor
    desc.opcode = DSA_OPCODE_MEMMOVE;
    desc.flags = IDXD_OP_FLAG_RCR  \
               | IDXD_OP_FLAG_CRAV \
               | IDXD_OP_FLAG_CC;
    desc.xfer_size = BLEN;
    desc.src_addr = (uintptr_t) src;
    desc.dst_addr = (uintptr_t) dst;
    desc.completion_addr = (uintptr_t) &comp;

retry:
    if (wq_info.wq_mapped) {
        submit_desc_check(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
        return EXIT_FAILURE;
    }
    
    int retry = 0;
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&comp);
        if (comp.status == 0) {
            uint64_t delay = rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }

    if (comp.status != DSA_COMP_SUCCESS) {
        if (op_status(comp.status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            // continue to finish the rest
            int wr = comp.status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp.fault_addr;
            wr ? *t = *t : *t;
            desc.src_addr += comp.bytes_completed;
            desc.dst_addr += comp.bytes_completed;
            desc.xfer_size -= comp.bytes_completed;
            goto retry;
        } else {
            printf("[error] desc failed status %u\n", comp.status);
            rc = EXIT_FAILURE;
        }
    } else {
        rc = memcmp(src, dst, BLEN);
        // rc ? printf("memmove failed: %d\n", rc) : printf("memmove successful\n");
        rc = rc ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    return rc;
}
