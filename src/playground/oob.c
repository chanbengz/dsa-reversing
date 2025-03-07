#include "dsa.h"
#include <stdint.h>
#include <stdio.h>

#define BLEN (4096 << 0)
#define TEST_NUM 10

uint64_t total_cycles = 0;

int main(int argc, char *argv[])
{
    char src[BLEN * TEST_NUM];
    char dst[BLEN * TEST_NUM];
    for (int i = 0; i < TEST_NUM; i++) memset(src + i * BLEN, 0x41 + i, BLEN);

    // skip the first, since it results in TLB miss
    submit_wd(src, dst);
    total_cycles = 0;

    for (int i = 1; i < TEST_NUM; i++) {
        int result = submit_wd(src + (BLEN << 1), dst + i * BLEN);
        if (result) return 1;
    }

    printf("BLEN: %d\n", BLEN);
    printf("avg cycles: %f\n", (double) total_cycles / (double) (TEST_NUM - 1));

    return 0;
}

static inline void submit_desc_check(void *wq_portal, struct dsa_hw_desc *hw)
{
    int result = enqcmd(wq_portal, hw);
    if (!result) return;

    while (enqcmd(wq_portal, hw)) { printf("Enq failed\n"); _mm_pause(); }
}

int submit_wd(void* src, void *dst) {
    /*printf("src: %p, dst: %p, char: %c\n", src, dst, *(char *)src);*/
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};
    uint32_t tlen;
    int rc;

    struct wq_info wq_info;
    rc = map_wq(&wq_info);
    if (rc) return EXIT_FAILURE;

    desc.opcode = DSA_OPCODE_MEMMOVE;
    /*
    * Request a completion – since we poll on status, this flag
    * needs to be 1 for status to be updated on successful
    * completion
    */
    desc.flags |= IDXD_OP_FLAG_RCR;
    /* CRAV should be 1 since RCR = 1 */
    desc.flags |= IDXD_OP_FLAG_CRAV;
    /* Hint to direct data writes to CPU cache */
    desc.flags |= IDXD_OP_FLAG_CC;
    desc.xfer_size = BLEN;
    desc.src_addr = (uintptr_t) src;
    desc.dst_addr = (uintptr_t) dst;
    desc.completion_addr = (uintptr_t)&comp;

retry:
    if (wq_info.wq_mapped) {
        submit_desc_check(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
        return EXIT_FAILURE;
    }
    
    // polling for completion
    uint64_t start = rdtsc();
    while (comp.status == 0);
    uint64_t end = rdtsc();
    total_cycles += end - start;
    printf("execution time: %ld\n", end - start);

    if (comp.status != DSA_COMP_SUCCESS) {
        if (op_status(comp.status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            int wr = comp.status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp.fault_addr;
            wr ? *t = *t : *t;
            desc.src_addr += comp.bytes_completed;
            desc.dst_addr += comp.bytes_completed;
            desc.xfer_size -= comp.bytes_completed;
            goto retry;
        } else {
            printf("desc failed status %u\n", comp.status);
            rc = EXIT_FAILURE;
        }
    } else {
        rc = memcmp(src, dst, BLEN);
        /*rc ? printf("memmove failed\n") : printf("memmove successful\n");*/
        rc = rc ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    return rc;
}
