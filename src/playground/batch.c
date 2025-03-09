#include "dsa.h"
#include <stdbool.h>

#define BLEN (4096 << 0)
#define BATCH_SIZE 16

bool async = false;
struct wq_info wq_info;
struct dsa_hw_desc* desc_buf;
struct dsa_completion_record* comp_buf;
struct dsa_completion_record batch_comp __attribute__((aligned(32)));
struct dsa_hw_desc batch_desc = {};

void submit_batch(void* src, void *dst)
{
    for (int i = 1; i < BATCH_SIZE; i++) {
        comp_buf[i].status          = 0;
        desc_buf[i].opcode          = DSA_OPCODE_MEMMOVE;
        desc_buf[i].flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
        desc_buf[i].xfer_size       = BLEN;
        desc_buf[i].src_addr        = (uintptr_t)&src[i];
        desc_buf[i].dst_addr        = (uintptr_t)&dst[i];
        desc_buf[i].completion_addr = (uintptr_t)&(comp_buf[i]);
    }

    batch_comp.status          = 0;
    batch_desc.opcode          = DSA_OPCODE_BATCH;
    batch_desc.flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    batch_desc.desc_count      = BATCH_SIZE;
    batch_desc.desc_list_addr  = (uint64_t)&(desc_buf[0]);
    batch_desc.completion_addr = (uintptr_t)&batch_comp;

    uint64_t start = rdtsc();
    // polling for completion
    int retry = 0;
    while (batch_comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&batch_comp);
        if (batch_comp.status == 0) {
            uint64_t delay = rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    uint64_t end = rdtsc();
    printf("[batch] time elapsed: %ld\n", end - start);
}

int main(int argc, char *argv[])
{
    char *src = (char *)malloc(BLEN * BATCH_SIZE), *dst = (char *)malloc(BLEN * BATCH_SIZE);
    for (int i = 0; i < BATCH_SIZE; i++) memset(src + i * BLEN, 0x41 + i, BLEN);

    desc_buf = (struct dsa_hw_desc *)aligned_alloc(64, BATCH_SIZE * sizeof(struct dsa_hw_desc));
    comp_buf = (struct dsa_completion_record *)aligned_alloc(64, BATCH_SIZE * sizeof(struct dsa_completion_record));

    if (map_wq(&wq_info)) return EXIT_FAILURE;
    // skip the first, since it results in TLB miss
    submit_wd(src, dst);
    async = true;

    return 0;
}

int submit_wd(void* src, void *dst) {
    int rc;
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp = {};

    comp.status          = 0;
    desc.opcode          = DSA_OPCODE_MEMMOVE;
    desc.flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc.xfer_size       = BLEN;
    desc.src_addr        = (uintptr_t) src;
    desc.dst_addr        = (uintptr_t) dst;
    desc.completion_addr = (uintptr_t) &comp;

    // submit
retry:
    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
        return EXIT_FAILURE;
    }

    if (async) return EXIT_SUCCESS;

    uint64_t start = rdtsc();
    // polling for completion
    int retry = 0;
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&comp);
        if (comp.status == 0) {
            uint64_t delay = rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    uint64_t end = rdtsc();
    printf("[single] time elapsed: %ld\n", end - start);

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
            rc = EXIT_FAILURE;
        }
    } else {
        rc = EXIT_SUCCESS;
    }
    return rc;
}
