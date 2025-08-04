# Playground of DSA

This directory contains the experiments described in the Section IV "Reverse Engineering DSA".

## Reverse Engineering ATC (DevTLB)

### Overview

This directory contains the reverse-engineering source code of the ATC (Address Translation Cache) in DSA. The ATC is device-side TLB (DevTLB) that caches the temporary address translation entries for DSA operations. If ATC hit, the DSA can access the memory directly without going through the host CPU's page table, i.e., via physical address.

The reverse-engineering dissects the ATC's structure via built-in PMU (Performance Monitoring Unit) and identifies potential side-channel attacks. To find more details, see our paper for Section 4 "DevTLB Structure and Policy".

### Perfmon

| Category | Event | Description                                               |
| -------- | ----- | --------------------------------------------------------- |
| 0x0      | 0x1   | Number of submitted descriptors to SWQ via limited portal |
| 0x0      | 0x10  | Number of submitted descriptors to DWQ                    |
| 0x2      | 0x40  | Number of translation requests to ATC                     |
| 0x2      | 0x80  | Number of translation requests not allocated ATC entry    |
| 0x2      | 0x100 | Number of translation requests hit ATC entry              |

For example,

```bash
sudo perf stat -e dsa0/filter_eng=0x1, event=0x100, event_category=0x2/ -a ./build/<program>
```

- `dsa0` is the DSA device. If you have multiple sockets, another device may be `dsa2`, `dsa4`, etc.
- `event=0x100, event_category=0x2` is the `0x100` event in category `0x2`.
- (Optional) `filter_eng=0x1` are the filters for engine number. This filter implies that some components are dedicated to specific engines.

### Experiments

Below are the experiments in the order of the paper.

> [!NOTE]
> `enqcmd` works the same as `write` for probing the ATC/DevTLB, so for convenience, we use `enqcmd` in the code snippets below.
> If you're strict about the "unprivileged" access, you can use `write` instead. Change `enqcmd` to the following snippet
> ```c
>   if (wq_info.wq_mapped) { enqcmd(wq_info.wq_portal, &desc); }
>   else { write(wq_info.wq_fd, &desc, sizeof(desc)); }
> ```

Setup the DSA with

```bash
cd $(ROOTDIR)/setup
sudo ./set_dsa.sh config/one_engine.conf

# For E2 in Fig. 5
sudo ./set_dsa.sh config/two_engines.conf
```

To replicate the experiments, build and run with `perf`. For example, to run the `rev` in the table

```bash
make build TARGET=rev
sudo perf stat -e dsa0/event=0x100,event_category=0x2/ -a ./build/rev
```

| Source | Description | What it does | Expected Result |
|--------|-------------|--------------|-----------------|
| [rev.c](atc/rev.c) | Listing 2. Capacity of DevTLB & Fig. 4 Latency distribution | Trying to evict DevTLB with only one entry. Two experiments involved in this code: 1. Listing 2 with multiples times; 2. Addresses in different memory region (e.g. BSS, DATA, Heap/Stack) | You can see latency distribution shown in Fig. 4 with `./build/rev 2` and also 10000 hits with `perf` |
| [readwrite.c](atc/readwrite.c) & [cmpval.c](atc/cmpval.c) | Listing 3. Primitive of `cmpval` with multiple addresses | Repeat `cmpval` (compare a src address with 8 bytes pattern) and `memcpy` (copy src to dst, read and write) | You can see 7 hits in `readwrite.c`, and the source has noted which line of descriptor causes the hits. Comment them to see the difference. `cmpval.c` tests the latency difference, which we found is no better than `noop`. |
| [dualcast.c](atc/dualcast.c) & [overlapping.c](atc/overlapping.c) | Listing 4. Primitive of `dualcast` for testing multiple descriptor fields | Examined overlapping fields in descriptor (src2 and dst). `dualcast.c` issues two writes to dst and dst2, twice. `overlapping.c` executes `memcpy` (src, dst, comp) and `memcmp` (src, src2, comp) alternately | `dualcast.c` causes 4 hits (src, dst, dst2, comp), and `overlapping.c` causes 8 hits (2 + 3 + 3, denoted in source code) |
| [crosspage.c](atc/crosspage.c) | Cross page behavior | Issued a cross-page `cmpval`, and then a following `cmpval` | If second probe is `probe(probe_arr + 16)`, 1 hit for completion record; if `probe(probe_arr + 4096)` (same as the last segment of previous), 2 hits are found |
| [victim.c](atc/victim.c) & [evict.c](atc/evict.c) | Figure 5. Eviction of ATC entries | Examined cross process isolation by PASID. Run `victim` in one terminal and `evict` in another. Beware of the WQ-Engine setup in Figure 5. | For `one_engine.conf` (E0 and E1), `./build/evict 0` (same WQ) and `./build/evict 1` (different WQ) both cause eviction because victim shows an increase in latency. For `two_engines.conf` (E2), no eviction happened if `./build/evict 1`. |
| [batch.c](atc/batch.c) | Batch fetcher's usage of devTLB | Check DevTLB hits/evictions by batch descriptor. | Hits are `BATCH_SUBMIT * BATCH_SIZE - 1` because writes to completion record of batch descriptor and fetchs of work descriptor cause no eviction and hit. |
| [offset.c](atc/offset.c) | Tweak bits in addresses to RE DevTLB | Change all bits one by one and measure the latency. | No latency variantion. |

## Work Queues (SWQ)

### Overview

This experiment benchmarks the latencies of submission and completion and reverse-engineers the arbiter policy between work and batch descriptors. Finally, we implemented an automatic discovery of the maximum window size of congestion.

### Experiments

| Source | Description | What it does | Expected Result |
|--------|-------------|--------------|-----------------|
| [benchmark.c](wq/benchmark.c) & [async.c](wq/async.c) | Figure 6. Benchmarking the latencies of submission and completion | Measure the latencies of each step in DSA workflows (submission and execution). | Results are similar to Fig. 6 in the paper. |
| [batch.c](wq/batch.c) | Listing 5. Reverse-engineering the arbiter policy between work and batch descriptors | Submit batch and work descriptors with different order and minimal interval, sequentially. | No significant difference in latency observed. 700 cycles due to memory latency. |
| [congest.c](wq/congest.c) | Automatic discovery of the maximum window size of congestion | Binary search on the feasible window sizes. Ideally, the outputs are the maximum window size. | Maximum window sizes of transfer size 2^15, 2^16, ..., 2^28. |

## Deprecated

Those experiments below proved that some of our hypothesis are wrong, or they do not concern the paper, so they are deprecated.

You can ignore this part but if you are interested, please feel free to check them out.

- [Constant Time Test](deprecated/cmp-flush.c) Test `Compare` and `Flush` to see if they are implemented in constant time. Experiment showed that `Compare` is constant time, but `Flush` is not. Using `Flush` to implement Flush+Reload and Flush+Flush is the same as using `clflush` but could be less accurate and slower.
- [Out of Bound Read/Write](deprecated/bound.c) Try `Memmory Move` with out-of-bound accesses. Results showed that it is possible to read/write memory outside of the buffer, similar to libc's `memcpy`.
- [Cache Timing](deprecated/cache-ctl.c) Test DSA's DDIO behavior with cache timing. Results showed that DSA will write to LLC even if Cache Control is disabled and will write to L2 cache if Cache Control is enabled. If the memory is not in cache, DSA will read/write to memory directly.