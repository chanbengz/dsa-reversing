# Playground of DSA

## Experiements

We tested if contention can be established in shared work queues (SWQ),
with similar approaches as in the reverse-engineer of ROB. The methodology is
pretty straightforward:
1. Submit

As we were not sured about the feasibility, we tried at first with fulling
the work queue with outstanding work descriptors. Then we started another process
to submit work descriptors to the work queue to see if it would encounter a failure.

Next, we 

## Deprecated

Those experiments below proved that some of our hypothesis are wrong,
or they do not concern the paper, so they are deprecated.

- [Constant Time Test](./cmp-flush.c) Test `Compare` and `Flush` to see if they are implemented in constant time. Experiment showed that `Compare` is constant time, but `Flush` is not. Using `Flush` to implement Flush+Reload and Flush+Flush is the same as using `clflush` but could be less accurate and slower.
- [Out of Bound Read/Write](./bound.c) Try `Memmory Move` with out-of-bound accesses. Results showed that it is possible to read/write memory outside of the buffer, similar to libc's `memcpy`.