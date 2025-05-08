#include "dsa.h"

#define BLEN (256)
#define TEST_NUM 8
struct wq_info wq_info;

int main(int argc, char *argv[])
{
    char *src = malloc(BLEN), *dst = malloc(BLEN);
    srand(114514LL);

    if (map_wq(&wq_info)) return EXIT_FAILURE;

    if (BLEN < 4096 && ((uintptr_t)src & (~0xFFF)) != ((uintptr_t)dst & (~0xFFF))) {
        printf("src and dst are in the same page\n");
        return EXIT_FAILURE;
    }

    memset(src, rand(), BLEN);
    memset(dst, 0, BLEN);

    int rc = submit_wd(src, dst);
    rc = submit_wd(src, dst);

    return rc;
}

int submit_wd(void* src, void *dst) {
    int rc;
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32)));
    
    // prepare descriptor
    comp.status = 0;
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
        submit_desc(wq_info.wq_portal, &desc);
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
            printf("Page fault\n");
            int wr = comp.status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp.fault_addr;
            wr ? *t = *t : *t;
            desc.src_addr += comp.bytes_completed;
            desc.dst_addr += comp.bytes_completed;
            desc.xfer_size -= comp.bytes_completed;
            goto retry;
        } else {
            printf("[error] desc failed status 0x%x\n", comp.status);
            rc = EXIT_FAILURE;
        }
    } else {
        rc = memcmp(src, dst, BLEN);
        rc = rc ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    return rc;
}
