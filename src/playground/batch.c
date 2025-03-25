#include "dsa.h"
#include <stdint.h>


#define BLEN (4096 << 0)
#define BATCH_SIZE 8

struct wq_info wq_info;
struct dsa_hw_desc *desc_buf, batch_desc __attribute__((aligned(64))), desc __attribute__((aligned(64)));
struct dsa_completion_record *comp_buf, batch_comp __attribute__((aligned(32))), comp __attribute__((aligned(32)));
uint64_t start;

void poll_batch()
{
    int retry = 0;
    uint64_t ba_wait;
    while (batch_comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&batch_comp);
        if (batch_comp.status == 0) {
            uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    ba_wait = rdtsc() - start;
    printf("[batch] time elapsed: %ld\n", ba_wait);
}

void poll_single()
{ 
    int retry = 0;
    uint64_t wd_wait;
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&comp);
        if (comp.status == 0) {
            uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    wd_wait = rdtsc() - start;
    printf("[single] time elapsed: %ld\n", wd_wait);
}

int main(int argc, char *argv[])
{
    char* src0 = malloc(BLEN), *dst0 = malloc(BLEN);
    char* src1 = malloc(BLEN * BATCH_SIZE), *dst1 = malloc(BLEN * BATCH_SIZE);
    memset(src0, 0xCB, BLEN); memset(src1, 0xCB, BLEN * BATCH_SIZE);
    desc_buf = (struct dsa_hw_desc *)aligned_alloc(64, sizeof(struct dsa_hw_desc) * BATCH_SIZE);
    comp_buf = (struct dsa_completion_record *)aligned_alloc(32, sizeof(struct dsa_completion_record) * BATCH_SIZE);

    if (map_wq(&wq_info)) return 1;

    comp.status          = 0;
    desc.opcode          = DSA_OPCODE_MEMMOVE;
    desc.flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc.xfer_size       = BLEN;
    desc.src_addr        = (uintptr_t)src0;
    desc.dst_addr        = (uintptr_t)dst0;
    desc.completion_addr = (uintptr_t)&comp;
    for (int i = 0; i < BATCH_SIZE; i++) {
        comp_buf[i].status          = 0;
        desc_buf[i].opcode          = DSA_OPCODE_MEMMOVE;
        desc_buf[i].flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
        desc_buf[i].xfer_size       = BLEN;
        desc_buf[i].src_addr        = (uintptr_t)src1 + i * BLEN;
        desc_buf[i].dst_addr        = (uintptr_t)dst1 + i * BLEN;
        desc_buf[i].completion_addr = (uintptr_t)&(comp_buf[i]);
    }
    batch_comp.status          = 0;
    batch_desc.opcode          = DSA_OPCODE_BATCH;
    batch_desc.flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    batch_desc.desc_count      = BATCH_SIZE;
    batch_desc.desc_list_addr  = (uint64_t)desc_buf;
    batch_desc.completion_addr = (uintptr_t)&batch_comp;

    enqcmd(wq_info.wq_portal, &desc);
    enqcmd(wq_info.wq_portal, &batch_desc);
    while(batch_comp.status == 0 || comp.status == 0);

    // batch first and then single
    comp.status = batch_comp.status = 0;
    for (int i = 0; i < BATCH_SIZE; i++) comp_buf[i].status = 0;
    enqcmd(wq_info.wq_portal, &batch_desc);
    _mm_sfence();
    enqcmd(wq_info.wq_portal, &desc);
    start = rdtsc();
    poll_single();

    // single first and then batch
    comp.status = batch_comp.status = 0;
    for (int i = 0; i < BATCH_SIZE; i++) comp_buf[i].status = 0;
    enqcmd(wq_info.wq_portal, &desc);
    start = rdtsc();
    _mm_sfence();
    enqcmd(wq_info.wq_portal, &batch_desc);
    poll_single();

    return 0;
}
