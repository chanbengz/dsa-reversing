#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/cdefs.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/idxd.h>
#include <xmmintrin.h>
#include <x86intrin.h>
#include <accel-config/libaccel_config.h>

#define NOP_RETRY 10000
#define MAX_COMP_RETRY 2000000000
#define UMWAIT_DELAY 100000
#define UMWAIT_STATE_C0_2 0
#define UMWAIT_STATE_C0_1 1


struct wq_info {
    bool wq_mapped;
    void *wq_portal;
    int wq_fd;
};

static inline int enqcmd(volatile void *reg, struct dsa_hw_desc *desc)
{
    uint8_t retry;
    asm volatile (".byte 0xf2, 0x0f, 0x38, 0xf8, 0x02\t\n"
                  "setz %0\t\n":"=r" (retry):"a"(reg), "d"(desc));
    return (int)retry;
}

static inline void submit_desc(void *wq_portal, struct dsa_hw_desc *hw)
{
    while (enqcmd(wq_portal, hw)) _mm_pause();
}

static bool is_write_syscall_success(int fd)
{
    struct dsa_hw_desc desc = {0};
    struct dsa_completion_record comp __attribute__((aligned(32)));
    int retry = 0, rc;

    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    comp.status = 0;
    desc.completion_addr = (unsigned long)&comp;

    rc = write(fd, &desc, sizeof(desc));
    if (rc == sizeof(desc)) {
        while (comp.status == 0 && retry++ < NOP_RETRY) _mm_pause();
        if (comp.status == DSA_COMP_SUCCESS)
            return true;
    }
    return false;
}

static int map_wq(struct wq_info *wq_info)
{
    void *wq_portal;
    struct accfg_ctx *ctx;
    struct accfg_wq *wq;
    struct accfg_device *device;
    char path[PATH_MAX];
    int fd;
    int wq_found;
    wq_portal = MAP_FAILED;
    accfg_new(&ctx);
    accfg_device_foreach(ctx, device) {
        /*
         * Use accfg_device_(*) functions to select enabled device
         * based on name, numa node
         */
        accfg_wq_foreach(device, wq) {
            if (accfg_wq_get_user_dev_path(wq, path, sizeof(path))) continue;
            /*
             * Use accfg_wq_(*) functions select WQ of type
             * ACCFG_WQT_USER and desired mode
             */
            wq_found = accfg_wq_get_type(wq) == ACCFG_WQT_USER;
            if (wq_found) break;
        }
        if (wq_found) break;
    }
    accfg_unref(ctx);
    if (!wq_found) return -ENODEV;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        wq_portal = mmap(NULL, 0x1000, PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
    }
    if (wq_portal == MAP_FAILED) {
        /*
         * EPERM means the driver doesn't support mmap but
         * can support write syscall. So fallback to write syscall
         */
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


static int map_spec_wq(struct wq_info *wq_info, char* path) {
    void *wq_portal;
    struct accfg_ctx *ctx;
    struct accfg_wq *wq;
    struct accfg_device *device;
    int fd;
    wq_portal = MAP_FAILED;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        wq_portal =
            mmap(NULL, 0x1000, PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
    }

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

static __always_inline uint64_t rdtsc(void) {
    uint64_t tsc;
    unsigned int dummy;
    tsc = __rdtscp(&dummy);
    __builtin_ia32_lfence();
    return tsc;
}

static __always_inline void flush(void *p) {
    asm volatile("clflush 0(%0)\n" : : "c"(p) : "rax");
}

static __always_inline void umonitor(void *addr) {
    asm volatile(".byte 0xf3, 0x48, 0x0f, 0xae, 0xf0" : : "a"(addr));
}

static __always_inline unsigned char
umwait(unsigned int state, unsigned long long timeout) {
    uint8_t r;
    uint32_t timeout_low = (uint32_t)timeout;
    uint32_t timeout_high = (uint32_t)(timeout >> 32);
    asm volatile(
        ".byte 0xf2, 0x48, 0x0f, 0xae, 0xf1\t\n"
        "setc %0\t\n" :
        "=r"(r) :
        "c"(state), "a"(timeout_low), "d"(timeout_high)
    );
    return r;
}

int submit_wd(void*, void*);

static inline void nsleep(uint64_t ns) {
    uint64_t start = rdtsc();
    while (rdtsc() - start < ns) {
        asm volatile("" : : : "memory");
    }
}

static uint8_t op_status(uint8_t status) {
    return status & DSA_COMP_STATUS_MASK;
}