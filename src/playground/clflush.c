#include "dsa.h"

#define BLEN 4096
#define TEST_NUM 10


int main(int argc, char *argv[])
{
    char src[BLEN];
    int dst_len = BLEN >> 2;
    srand(0LL);

    printf("identical\n");
    char randbyte = rand() % 128;
    memset(src, randbyte, BLEN);

    int result = submit_wd(src, &dst_len);

    return result;
}

int submit_wd(void* src, void *dst) {
    /*printf("src: %p, dst: %p, char: %c\n", src, dst, *(char *)src);*/
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};
    uint32_t tlen; int rc;

    struct wq_info wq_info;
    rc = map_wq(&wq_info);
    if (rc) return EXIT_FAILURE;

    desc.opcode = DSA_OPCODE_COMPARE;
    desc.flags |= IDXD_OP_FLAG_RCR;
    desc.flags |= IDXD_OP_FLAG_CRAV;
    // desc.flags |= IDXD_OP_FLAG_CC;
    desc.xfer_size = *(int *)dst;
    desc.dst_addr = (uintptr_t) src;
    desc.completion_addr = (uintptr_t)&comp;

retry:
    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
        return EXIT_FAILURE;
    }
    
    _mm_mfence();
    uint64_t start = __rdtsc();
    _mm_mfence();
    // polling for completion
    while (comp.status == 0);
    _mm_mfence();
    uint64_t end = __rdtsc();
    _mm_mfence();

    printf("time elapsed: %ld\n", end - start);

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
            printf("bytes different at %d\n", comp.bytes_completed);
            rc = EXIT_FAILURE;
        }
    } else {
        rc = EXIT_SUCCESS;
    }

    return rc;
}
