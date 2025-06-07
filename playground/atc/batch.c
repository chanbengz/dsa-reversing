#include "dsa.h"

#define BLEN (4096 >> 10)
#define BATCH_SIZE 3
#define BATCH_SUBMIT 2

struct wq_info wq_info;
struct dsa_hw_desc *desc_buf, batch_desc __attribute__((aligned(64)));
struct dsa_completion_record *comp_buf, batch_comp __attribute__((aligned(32)));

int main(int argc, char *argv[]) {
    desc_buf = (struct dsa_hw_desc *)aligned_alloc(
        64, sizeof(struct dsa_hw_desc) * BATCH_SIZE);
    comp_buf = (struct dsa_completion_record *)aligned_alloc(
        32, sizeof(struct dsa_completion_record) * BATCH_SIZE);
    char* src = (char *)aligned_alloc(4096, BLEN * BATCH_SIZE);
    memset(src, 0x0, BLEN * BATCH_SIZE);
    
    if (map_wq(&wq_info)) return 1;

    for (int i = 0; i < BATCH_SIZE; i++) {
        comp_buf[i].status = 0;
        desc_buf[i].opcode = DSA_OPCODE_NOOP;
        desc_buf[i].flags =
            IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_FENCE;
        desc_buf[i].completion_addr = (uintptr_t) & (comp_buf[i]);
        printf("comp_buf[%2d] = %p\n", i, &comp_buf[i]);
        /*
        desc_buf[i].opcode          = DSA_OPCODE_COMPVAL;
        desc_buf[i].src_addr        = (uintptr_t) src + i * BLEN;
        desc_buf[i].comp_pattern    = 0;
        desc_buf[i].expected_res    = 0;
        desc_buf[i].xfer_size       = BLEN;
        */
    }
    printf("batch_comp   = %p\n", &batch_comp);

    batch_desc.opcode = DSA_OPCODE_BATCH;
    batch_desc.flags = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    batch_desc.desc_count = BATCH_SIZE;
    batch_desc.desc_list_addr = (uint64_t) desc_buf;
    batch_desc.completion_addr = (uintptr_t) &batch_comp;

    for (int i = 0; i < BATCH_SUBMIT; i++) {
        // cleanup
        batch_comp.status = 0;
        for (int j = 0; j < BATCH_SIZE; j++) comp_buf[j].status = 0;
        // submit batch
        enqcmd(wq_info.wq_portal, &batch_desc);
        while (batch_comp.status == 0);
        if (batch_comp.status != 1) {
            printf("Batch %d failed with status %d\n", i, batch_comp.status);
            return -1;
        }
    }

    return batch_comp.status == 1 ? 0 : -1;
}
