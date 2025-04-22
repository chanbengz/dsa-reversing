# DSA Reversing

This repos holds for artifacts of <paper-title>.

## Getting Started

> [!NOTE]
> We recommend using Ubuntu 24.04 LTS and Intel Xeon Scalable for this experiment.

Make sure the driver of DSA and IOMMU is properly installed and enabled during bootup.
The following command do output something.

```bash
# check if DSA is enabled
sudo dmesg | grep "idxd "
sudo lspci | grep 0b25
sudo lspci -vvv -s 6a:01.0 
# check if VT-d is supported
cat /sys/bus/dsa/devices/dsa0/pasid_enabled
```

Setup `libaccel-config`

```bash
sudo apt install libaccel-config-dev libaccel-config1
```

Enable DSA with configuration (`sudo` required), and grant read/write priviledge to user.
Most of data-centers enable DSA by default.

```bash
sudo ./setup/setup_dsa.sh setup/config/some_config.conf
# or manually
sudo accel-config load-config -c setup/config/some_config.conf -e
sudo chmod 766 /dev/dsa/*
```

## Playing with DSA

`playground` holds the experiment sources.
For example, in `playground/wq` we explored the work queue in DSA,
run them by

```bash
cd playground/wq && mkdir -p build
make run TARGET=<target-file-without-.c>

# debug
make debug TARGET=<target-file-without-.c>
```

In `playgroundatc`, we tested the internal structure of ATC (also known as Device TLB)
as part of DSA device. You can run it by

```bash
cd playground/atc && mkdir -p build
# same as above
```

## Attack

See [attack/README.md](attack/README.md) or [attack-rs/README.md](attack-rs/README.md).

## Issues

If you encounter any issue, please see [ISSUE.md](ISSUE.md) first.

Open an issue if it doesn't help.

## Reference

- Intel(R) Data Streaming Accelerator User Guide
- Intel(R) Data Streaming Accelerator Architecuture Specification
- https://github.com/intel/dsa-perf-micros