# DSA Reversing

This repos holds for experiment of Intel(R) Data Streaming Accelerator.

## Getting Started

> [!NOTE]
> We recommend using Ubuntu 24.04 LTS and Intel Xeon Scalable for this experiment.

Make sure the driver of DSA and IOMMU is properly installed and enabled during bootup.
The following command do output something.

```bash
sudo dmesg | grep "idxd "
sudo lspci | grep 0b25
sudo lspci -vvv -s 6a:01.0 

# Optional. Only if you want to transfer data across processes
cat /sys/bus/dsa/devices/dsa0/pasid_enabled
```

Setup `libaccel-config`

```bash
sudo apt install libaccel-config-dev libaccel-config1
```

Enable DSA with configuration (`sudo` required), and give read/write priviledge to user.
Most of data-centers enable DSA by default.

```bash
sudo accel-config load-config -c setup/contention_profile.conf -e
# or
sudo ./setup/setup_dsa.sh setup/some_profile.conf

sudo chmod 766 /dev/dsa/*
```

(Optional) Well, it's a bit of annoying that Linux enables capabilities by default, so
the `mmap` to DSA portal may failed. We give `cap_sys_rawio` to our "trusted" binaries

```bash
sudo setcap cap_sys_rawio+ep <your-binary>
```

## Playing with DSA

There're currently some code under `src/playground`, and run them by

```bash
cd src/playground && mkdir -p build
make run TARGET=<target-file-without-.c>

# debug
make debug TARGET=<target-file-without-.c>
```

## Reference

- Intel(R) Data Streaming Accelerator User Guide
- Intel(R) Data Streaming Accelerator Architecuture Specification
- https://github.com/intel/dsa-perf-micros 
