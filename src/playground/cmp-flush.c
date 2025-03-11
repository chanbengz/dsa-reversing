#define _GNU_SOURCE

#include "dsa.h"
#include <sys/sysinfo.h>
#include <pthread.h>
#include <sched.h>

#define BLEN 4096
#define TEST_NUM 10

struct wq_info wq_info;

int cpu_pin(uint32_t cpu)
{
	cpu_set_t *cpuset;
	size_t cpusetsize;

	cpusetsize = CPU_ALLOC_SIZE(get_nprocs());
	cpuset = CPU_ALLOC(get_nprocs());
	CPU_ZERO_S(cpusetsize, cpuset);
	CPU_SET_S(cpu, cpusetsize, cpuset);

	pthread_setaffinity_np(pthread_self(), cpusetsize, cpuset);

	CPU_FREE(cpuset);

	return 0;
}

int compare(void* src, void* dst);
int cflush(void* dst, int len);

int main(int argc, char *argv[])
{
    cpu_pin(118);
    if (map_wq(&wq_info)) return EXIT_FAILURE;

    /*
    char src[BLEN], dst[BLEN], result, randbyte;
    srand(0LL);

    printf("[compare] not identical\n");
    randbyte = rand() % 128;
    memset(src, randbyte, BLEN >> 1);
    memset(src + (BLEN >> 1), rand() % 128, BLEN >> 1);
    memset(dst, randbyte, BLEN);
    result = compare(src, dst);

    printf("[compare] identical\n");
    randbyte = rand() % 128;
    memset(src, randbyte, BLEN);
    memset(dst, randbyte, BLEN);
    result = compare(src, dst);
    */

    // ---- flush dst -----
    uint64_t* src_ptr = malloc(BLEN);
    memset(src_ptr, 0xCB, BLEN);
    for (int i = 0; i < BLEN / sizeof(uint64_t); i++) *(src_ptr + i) = i; 

    printf("[cflush] cache hit\n");
    cflush(src_ptr, BLEN);
    printf("[cflush] cache miss\n");
    cflush(src_ptr, BLEN);

    return 0;
}

int cflush(void *dst, int len) {
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};

    comp.status = 0;
    desc.opcode = DSA_OPCODE_CFLUSH;
    desc.flags = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc.dst_addr = (uintptr_t) dst;
    desc.xfer_size = len;
    desc.completion_addr = (uintptr_t)&comp;

retry:
    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
        return EXIT_FAILURE;
    }
    
    uint64_t start = rdtsc();
    int retry = 0;
    while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&comp);
        if (comp.status == 0) {
            uint64_t delay = rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    uint64_t end = rdtsc();
    printf("[cflush] time elapsed: %ld\n", end - start);

    return EXIT_SUCCESS;
}

int compare(void* src, void *dst) {
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};
    uint32_t tlen, rc;

    desc.opcode = DSA_OPCODE_COMPARE;
    desc.flags |= IDXD_OP_FLAG_RCR;
    desc.flags |= IDXD_OP_FLAG_CRAV;
    desc.flags |= IDXD_OP_FLAG_CR;
    desc.xfer_size = BLEN;
    desc.src_addr = (uintptr_t) src;
    desc.src2_addr = (uintptr_t) dst;
    desc.expected_res = 0;
    desc.completion_addr = (uintptr_t)&comp;

retry:
    if (wq_info.wq_mapped) {
        submit_desc(wq_info.wq_portal, &desc);
    } else {
        int rc = write(wq_info.wq_fd, &desc, sizeof(desc));
        if (rc != sizeof(desc))
        return EXIT_FAILURE;
    }
    
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
    printf("[compare] time elapsed: %ld\n", end - start);

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
            printf("[compare] bytes different at %d\n", comp.bytes_completed);
            rc = EXIT_FAILURE;
        }
    } else {
        rc = EXIT_SUCCESS;
    }

    return rc;
}
