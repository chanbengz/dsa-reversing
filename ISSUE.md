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

## `attack-rs` fail to reproduce

Rust implmentation is unstable now.