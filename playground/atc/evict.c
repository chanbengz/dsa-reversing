#include "dsa.h"
#include <linux/idxd.h>
#define TESTS_PER_PROFILE 1
#define BLEN (4096ull << 26) // 256GB
#define DSA_OP_FLAG_US (1 << 16)

struct dsa_completion_record *probe_arr;
struct dsa_completion_record arr_onbss = {};
struct wq_info wq_info;
struct dsa_hw_desc desc = {};

#define TESTS_PER_PROBE 4
#define MAX_OFFSET 22
uint64_t results[MAX_OFFSET + 1][TESTS_PER_PROBE];

int map_another_wq(struct wq_info *wq_info) {
    int fd = open("/dev/dsa/wq2.1", O_RDWR);
    void *wq_portal =
        mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (wq_portal == MAP_FAILED) {
        if (errno == EPERM && is_write_syscall_success(fd)) {
            wq_info->wq_mapped = false;
            wq_info->wq_fd = fd;
            return 0;
        }
        return -errno;
    }
    wq_info->wq_portal = wq_portal;
    wq_info->wq_mapped = true;
    wq_info->wq_fd = -1;
    return 0;
}

int main(int argc, char *argv[]) {
    struct dsa_completion_record arr_onstack __attribute__((aligned(32))) = {};
    probe_arr = (struct dsa_completion_record *)aligned_alloc(32, BLEN);
    memset(probe_arr, 0, BLEN >> 10);

    if (argc != 2) {
        printf("Usage: %s <another wq ? 0 : 1>\n", argv[0]);
        return 1;
    }

    if (atoi(argv[1]) == 1) {
        if (map_wq(&wq_info)) return EXIT_FAILURE;
    } else {
        if (map_another_wq(&wq_info)) return EXIT_FAILURE;
    }

    probe(probe_arr);
    // probe(&arr_onstack);
    // probe(&arr_onbss);

    return 0;
}

uint64_t probe(void *addr) {
    uint64_t start, end, retry = 0, ret = 0;
    struct dsa_completion_record *comp = (struct dsa_completion_record *)addr;
    probe_arr[0].status = 0;
    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    desc.completion_addr = (uintptr_t)comp;
    memset(comp, 0, 8);

resubmit:
    // enqcmd(wq_info.wq_portal, &desc);
    write(wq_info.wq_fd, &desc, sizeof(desc));
    start = rdtsc();
    while (comp->status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&(comp));
        if (comp->status == 0) {
            uint64_t delay = __rdtsc() + UMWAIT_DELAY;
            umwait(UMWAIT_STATE_C0_1, delay);
        }
    }
    end = rdtsc();
    ret += end - start;

    if (comp->status != DSA_COMP_SUCCESS) {
        printf("Failed: %d Bytes Completed: %d\n", comp->status,
               comp->bytes_completed);
        printf("Address: %p\n", (void *)comp->fault_addr);
        if (op_status(comp->status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            int wr = comp->status & DSA_COMP_STATUS_WRITE;
            volatile char *t;
            t = (char *)comp->fault_addr;
            wr ? *t = *t : *t;
            printf("retrying\n");
            goto resubmit;
        } else {
            printf("Error: %d\n", comp->status);
        }
    }
    return ret;
}
