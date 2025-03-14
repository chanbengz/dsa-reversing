#include "dsa.h"

#define BLEN (4096 << 0)

struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {};

int main(int argc, char *argv[])
{
    char *src, *dst, dummy;
    uint64_t start;
    src = malloc(BLEN), dst = malloc(BLEN);
    memset(src, 0x61, BLEN); // a
    memset(dst, 0x41, BLEN); // A

    int rc = map_wq(&wq_info);
    if (rc) return EXIT_FAILURE;
    comp.status          = 0;
    desc.opcode          = DSA_OPCODE_MEMMOVE;
    desc.flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc.xfer_size       = BLEN;
    desc.src_addr        = (uintptr_t) src;
    desc.dst_addr        = (uintptr_t) dst;
    desc.completion_addr = (uintptr_t) &comp;

    // cache hit
    start = rdtsc();
    dummy = *dst; 
    printf("cache hit: %4lu\t", rdtsc() - start);
    printf("dst: %c\n", dummy);

    // cache control off
    enqcmd(wq_info.wq_portal, &desc);
    while (comp.status == 0);
    start = rdtsc();
    dummy = *dst; 
    printf("cache nctl:%4lu\t", rdtsc() - start);
    printf("dst: %c\n", dummy);

    // cache control on
    comp.status          = 0;
    desc.flags           |= IDXD_OP_FLAG_CC;
    enqcmd(wq_info.wq_portal, &desc);
    while (comp.status == 0);
    start = rdtsc();
    dummy = *dst; 
    printf("cache ctl: %4lu\t", rdtsc() - start);
    printf("dst: %c\n", dummy);

    // cache flushed
    flush(dst);
    start = rdtsc();
    dummy = *dst;
    printf("cache fls: %4lu\n", rdtsc() - start);

    return 0;
}
