#include "dsa.h"

#define TEST_SIZE 1
#define BLEN (4096ull << 26) // 256GB
#define DSA_OP_FLAG_US (1 << 16)

struct dsa_completion_record* probe_arr;
struct wq_info wq_info;
struct dsa_hw_desc desc = {};
/*struct dsa_completion_record comp __attribute__((aligned(32))) = {};*/


uint64_t probe(void* addr)
{
    uint64_t start = 0, end = 0, retry = 0;
    struct dsa_completion_record* comp = (struct dsa_completion_record*) addr;

    // Initialize the descriptor
    comp->status            = 0;
    desc.completion_addr    = (uintptr_t) comp;
    // desc.src_addr          = (uintptr_t) addr;
    // memset(addr, 0, 8);

    enqcmd(wq_info.wq_portal, &desc);

    start = rdtsc();
    while (comp->status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&(comp));
        if (comp->status == 0) {
        uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    end = rdtsc();

    if (comp->status != DSA_COMP_SUCCESS) {
        printf("Failed: %d Bytes Completed: %d\n", comp->status, comp->bytes_completed);
        printf("Address: %p\n", (void *)comp->fault_addr);
        if (op_status(comp->status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            int wr = comp->status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp->fault_addr;
            wr ? *t = *t : *t;
            // goto retry;
        }
    }

    return end - start;
}

void profile(void* base, int w, int s)
{
    probe(base); // Miss
    // The base address is present in ATC
    for (int i = 0; i < TEST_SIZE; i++) {
        for (int j = 1; j <= w + 1; j++) {
            void* addr = base + j * s * 4096;
            printf("Probing %p\n", addr);
            probe(addr);
        }
    }
    // Hit:  not evicted
    // Miss: evicted
    probe(base); 
}

int main(int argc, char *argv[])
{
    uint64_t start, end;
    int retry = 0;

    probe_arr = (struct dsa_completion_record *)aligned_alloc(32, BLEN);
    if (map_wq(&wq_info)) return EXIT_FAILURE;
    // warm up
    probe_arr[0].status = 0;
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.completion_addr = (uintptr_t) &probe_arr[0];
    enqcmd(wq_info.wq_portal, &desc);
    start = rdtsc();
    while (probe_arr[0].status == 0) _mm_pause();
    end = rdtsc();
    // printf("Warmup: %ld\n", end - start);

    // Benchmarking ATC
    void* base = probe_arr;
    if (argc == 3) {
        int w = atoi(argv[1]);
        int s = atoi(argv[2]);
        printf("Testing W = %d, S = %d\n", w, s);
        profile(base + (0x7E << 5), w, s);
    } else if (argc == 2) {
        void *addr = base + (0x20 << atoi(argv[1]));
        probe(base); // Hit
        printf("Probing %p\n", addr);
        probe(addr);
        probe(base);
    }

    return 0;
}
