# Known Issues

## Unable to open Work Queue Portal

Well, it's a bit of annoying that Linux enables capabilities by default, so
the `mmap` to DSA portal may failed. We give `cap_sys_rawio` to our "trusted" binaries

```bash
sudo setcap cap_sys_rawio+ep <your-binary>
```