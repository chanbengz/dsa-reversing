#include "dsa.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BLEN (4096 << 0)

int main(int argc, char *argv[])
{
    char src[BLEN + 2], dst[BLEN];
    memset(src, 0x41, BLEN + 2);
    submit_wd(src, dst);
    printf("dst[BLEN + 1] = %c\n", dst[BLEN + 1]);

    return 0;
}

int submit_wd(void* src, void *dst) {
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};
    uint32_t tlen, rc;

    struct wq_info wq_info;
    rc = map_wq(&wq_info);
    if (rc) return EXIT_FAILURE;

    desc.opcode = DSA_OPCODE_MEMMOVE;
    desc.flags  = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc.xfer_size = BLEN + 1;
    desc.src_addr = (uintptr_t) src;
    desc.dst_addr = (uintptr_t) dst;
    desc.completion_addr = (uintptr_t)&comp;

retry:
    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
        return EXIT_FAILURE;
    }
    
    // polling for completion
    while (comp.status == 0);

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
        rc = rc ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    return rc;
}
