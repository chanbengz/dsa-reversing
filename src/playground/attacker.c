#include "util.h"


int main(int argc, char *argv[])
{
    char src[BLEN], dst[BLEN];
    memset(src, 0x41, BLEN);
    int result = submit_wd(src, dst);
    if (!result) for (int i = 0; i < 16; i++) printf("%c", dst[i]);

    return 0;
}

/*
 * flush wq non-blocking
 *
 */
int submit_wd(void* src, void *dst) {
    struct dsa_hw_desc desc = { };
    struct dsa_completion_record comp __attribute__((aligned(32)));
    uint32_t tlen;
    int rc;

    struct wq_info wq_info;
    rc = map_wq(&wq_info);
    if (rc) return EXIT_FAILURE;

    memset(src, 0x41, BLEN);
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
        printf("ENQCMD\n");
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        printf("SYSCALL\n");
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
        return EXIT_FAILURE;
    }
    while (comp.status == 0) _mm_pause();
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
        printf("desc successful\n");
        rc = memcmp(src, dst, BLEN);
        rc ? printf("memmove failed\n") : printf("memmove successful\n");
        rc = rc ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    return rc;
}
