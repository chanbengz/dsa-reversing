<div align=center>

# Data Stealth Assassin: Exploiting Intel Data Streaming Accelerator in Side-Channel Attacks

</div>

This repos holds the artifact of paper "Data Stealth Assassin: Exploiting Intel Data Streaming Accelerator in Side-Channel Attacks".

> [!WARNING]
> We're organizing the artifacts and docs for reproducing. Stay tuned.

## Structure

```
dsa-reversing
├── attack     # Attack experiments
├── evaluate   # Evaluation scripts
├── ISSUE.md   # FAQ
├── LICENSE
├── playground # Reverse-engineering
│   ├── atc    # DevTLB
│   └── wq     # Shared work queue
├── README.md
└── setup      # Environment setup
    ├── config
    ├── dto
    ├── vpp
    ├── Dockerfile.*
    └── setup_dsa.sh
```

## Getting Started

> [!NOTE]
> This experiment requires Ubuntu 24.04 LTS and Intel 4th Gen Xeon Scalable (or newer) who
> has DSA integration. Root privilege is required to setup and run the experiment.

Make sure the driver of DSA and VT-d is properly installed and configured during bootup. The following command do output something to verify that they're working.

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

```bash
sudo ./setup/setup_dsa.sh setup/config/some_config.conf
# or manually
sudo accel-config load-config -c setup/config/some_config.conf -e
sudo chmod 766 /dev/dsa/*
```

## Playing with DSA

`playground` holds the experiment sources. For example, in `playground/wq` you can find the source code of the workload queue (WQ) experiment.

```bash
cd playground/wq && mkdir -p build # or playground/atc
make buildall
```

See [playground/README.md](playground/README.md) for more details.

## Attack

See [attack/README.md](attack/README.md) for attack experiments.

## Issues

If you encounter any issue, please see [ISSUE.md](ISSUE.md) first. Open an issue if it doesn't help.

## Reference

- Intel(R) Data Streaming Accelerator User Guide
- Intel(R) Data Streaming Accelerator Architecuture Specification
- https://github.com/intel/dsa-perf-micros
- https://github.com/ligato/vpp-base

## Disclaimer

We provide this code as-is, for academic purpose only. You are responsible for protecting yourself, your property and privacy. Any risks or damage caused by this code will not be covered.
