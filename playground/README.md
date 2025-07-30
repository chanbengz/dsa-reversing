# Playground of DSA

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

1. [offset.c](atc/offset.c) & [rev.c](atc/rev.c): Listing 2. Capacity of ATC.



2. [readwrite.c](atc/readwrite.c) & [cmpval.c](atc/cmpval.c): Listing 3. Primitive of `cmpval` with multiple addresses.



3. [dualcast.c](atc/dualcast.c) & [overlapping](atc/overlapping.c): Listing 4. Primitive of `dualcast` for testing multiple descriptor fields.



4. [crosspage.c](atc/crosspage.c): Cross page behavior.



5. [victim.c](atc/victim.c) & [evict.c](atc/evict.c): Figure 5. Eviction of ATC entries.



## Work Queues (SWQ)

### Overview

This experiment benchmarks the latencies of submission and completion and reverse-engineers the arbiter policy between work and batch descriptors. Finally, we implemented an automatic discovery of the maximum window size of congestion.

### Experiments

1. [benchmark.c](wq/benchmark.c) & [async.c](wq/async.c): Figure 6. Benchmarking the latencies of submission and completion.



2. [batch.c](wq/batch.c): Listing 5. Reverse-engineering the arbiter policy between work and batch descriptors.



3. [congest.c](wq/congest.c): Automatic discovery of the maximum window size of congestion.



## Deprecated

Those experiments below proved that some of our hypothesis are wrong,
or they do not concern the paper, so they are deprecated.

- [Constant Time Test](./cmp-flush.c) Test `Compare` and `Flush` to see if they
are implemented in constant time. Experiment showed that `Compare` is constant time,
but `Flush` is not. Using `Flush` to implement Flush+Reload and Flush+Flush is the
same as using `clflush` but could be less accurate and slower.
- [Out of Bound Read/Write](./bound.c) Try `Memmory Move` with out-of-bound accesses.
Results showed that it is possible to read/write memory outside of the buffer, similar
 to libc's `memcpy`.
