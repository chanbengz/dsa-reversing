#include "dsa.h"

#define BLEN (4096)
#define XFER_SIZE 8
#define TEST_NUM 8
struct wq_info wq_info;

int main(int argc, char *argv[]) {
    char *src = malloc(BLEN), *dst = malloc(BLEN * 3);
    srand(114514LL);

    if (map_wq(&wq_info)) return EXIT_FAILURE;

    // if BLEN = 4096, we show that read/write are using different pages/caches
    if (BLEN < 4096 &&
        ((uintptr_t)src & (~0xFFF)) != ((uintptr_t)dst & (~0xFFF))) {
        printf("src and dst are not in the same page\n");
        return EXIT_FAILURE;
    }

    memset(src, rand(), BLEN);
    memset(dst, 0, BLEN * 3);

    int rc = submit_wd(src, dst);
    rc |= submit_wd(src, dst + 4096); // hit 2 times
    rc |= submit_wd(src, dst);        // hit 2 times
    rc |= submit_wd(src, dst);        // hit 3 times

    return rc;
}

int submit_wd(void *src, void *dst) {
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32)));

    if (((uintptr_t)src & (~0xFFF)) == ((uintptr_t)dst & (~0xFFF)) ||
        ((uintptr_t)src & (~0xFFF)) == ((uintptr_t)(&comp) & (~0xFFF))) {
        printf("They cannot be in the sage paeg");
        return 1;
    }

    // prepare descriptor
    comp.status = 0;
    desc.opcode = DSA_OPCODE_MEMMOVE;
    desc.flags = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_CC;
    desc.xfer_size = XFER_SIZE;
    desc.src_addr = (uintptr_t)src;
    desc.dst_addr = (uintptr_t)dst;
    desc.completion_addr = (uintptr_t)&comp;

    int rc;
retry:
    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc)) return EXIT_FAILURE;
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
        rc = memcmp(src, dst, XFER_SIZE);
        rc = rc ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    return rc;
}
