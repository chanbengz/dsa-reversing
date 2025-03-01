#define _GNU_SOURCE

#include "dsa.h"
#include <sys/sysinfo.h>
#include <pthread.h>
#include <sched.h>

#define BLEN 4096
#define TEST_NUM 10

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


int main(int argc, char *argv[])
{
    cpu_pin(118);
    char src[BLEN], dst[BLEN], result, randbyte;
    srand(0LL);

    printf("not identical\n");
    randbyte = rand() % 128;
    memset(src, randbyte, BLEN >> 1);
    memset(src + (BLEN >> 1), rand() % 128, BLEN >> 1);
    memset(dst, randbyte, BLEN);

    result = submit_wd(src, dst);

    printf("identical\n");
    randbyte = rand() % 128;
    memset(src, randbyte, BLEN);
    memset(dst, randbyte, BLEN);

    result = submit_wd(src, dst);

    return 0;
}

/*
 * flush wq non-blocking
 *
 */

int submit_wd(void* src, void *dst) {
    /*printf("src: %p, dst: %p, char: %c\n", src, dst, *(char *)src);*/
    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32))) = {};
    uint32_t tlen;
    int rc;

    struct wq_info wq_info;
    rc = map_wq(&wq_info);
    if (rc) return EXIT_FAILURE;

    desc.opcode = DSA_OPCODE_COMPARE;
    /*
    * Request a completion – since we poll on status, this flag
    * needs to be 1 for status to be updated on successful
    * completion
    */
    desc.flags |= IDXD_OP_FLAG_RCR;
    /* CRAV should be 1 since RCR = 1 */
    desc.flags |= IDXD_OP_FLAG_CRAV;
    /* Hint to check result if they're identical */
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
