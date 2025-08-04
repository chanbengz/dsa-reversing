# Known Issues

## Unable to open Work Queue Portal

Well, it's a bit of annoying that Linux enables capabilities by default, so
the `mmap` to DSA portal may failed. We give `cap_sys_rawio` to our "trusted" binaries

```bash
sudo setcap cap_sys_rawio+ep <your-binary>
```

This may fail. If failed, please turn to `write`. Some of the code may use only the `enqcmd`,
so you will need to modify that manually.

## Segmentation fault

Note that completion record must be 32-byte aligned, otherwise it will cause segmentation fault.

## Page fault

If you assign an allocated buffer to descriptor using `malloc`, it may cause (soft/minor) page fault.
However, PRS cannot deal with soft page fault because page tables have not mapping to them. PRS can
only do the swap-in of hard page fault. Thus, ensure that `malloc`ed buffers are accessed in process
before being accessed by DSA.

## Failed to Build Docker Image

If it's network issue (like if you are in China), the fastest way is to use a proxy. After setting up
a local proxy (like Clash/Shadowsocks), you can set the `http_proxy` and `https_proxy` environment variables
and add `--network host` and `--build-arg http_proxy=$http_proxy` to the `docker build` command.

## Calibration Keeps Failing or the Latency Varies Too Much

This would happen if you running on a server with NUMA architecture (multiple CPU sockets).
You can try to set the CPU affinity of the DSA process to a single CPU core using
```bash
taskset -c <core_id> <your_dsa_process>
```

To get the core ID, you can use `numactl -H` to list the available cores and their NUMA nodes.
Normally, `dsa0` will locate on the numa node 0, and `dsa2` will locate on the numa node 1.

## Experiments Failed to Replicate with Expected Results

For reverse-engineering experiments, ensure that you have the DSA 1.0 on 4th or 5th Xeon. The results are fixed in these environments.
Report the issue if you found any unexpected results or they're inconsistent with paper/documentation in this artifact.

For case-study attacks. You need to tune the parameters in the code to match your environment. Tuning guide can be found in the README of `attack` directory.