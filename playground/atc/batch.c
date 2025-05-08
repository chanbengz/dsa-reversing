#include "dsa.h"

#define BLEN (4096 << 0)
#define BATCH_SIZE 2
#define BATCH_SUBMIT 2

struct wq_info wq_info;
struct dsa_hw_desc *desc_buf, batch_desc __attribute__((aligned(64)));
struct dsa_completion_record *comp_buf, batch_comp __attribute__((aligned(32)));

int main(int argc, char *argv[])
{
    desc_buf = (struct dsa_hw_desc *)aligned_alloc(64, sizeof(struct dsa_hw_desc) * BATCH_SIZE);
    comp_buf = (struct dsa_completion_record *)aligned_alloc(32, sizeof(struct dsa_completion_record) * BATCH_SIZE);
    if (map_wq(&wq_info)) return 1;

    for (int i = 0; i < BATCH_SIZE; i++) {
        comp_buf[i].status          = 0;
        desc_buf[i].opcode          = DSA_OPCODE_NOOP;
        desc_buf[i].flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
        desc_buf[i].completion_addr = (uintptr_t) &(comp_buf[i]);
        /*
        desc_buf[i].opcode          = DSA_OPCODE_MEMMOVE;
        desc_buf[i].src_addr        = (uintptr_t) src + i * BLEN; // cross page
        desc_buf[i].dst_addr        = (uintptr_t) dst + i * BLEN; // cross page
        desc_buf[i].xfer_size       = BLEN;
        */
    }

    batch_desc.opcode          = DSA_OPCODE_BATCH;
    batch_desc.flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    batch_desc.desc_count      = BATCH_SIZE;
    batch_desc.desc_list_addr  = (uint64_t) desc_buf;
    batch_desc.completion_addr = (uintptr_t) &batch_comp;
    
    printf("batch buffer: %p\n", desc_buf);

    for (int i = 0; i < BATCH_SUBMIT; i++) {
        batch_comp.status = 0;
        enqcmd(wq_info.wq_portal, &batch_desc);
        while(batch_comp.status == 0);
    }

    return batch_comp.status == 1 ? 0 : -1;
}
