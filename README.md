<div align=center>

# DSAssassin: Cross-VM Side-Channel Attacks by Exploiting Intel Data Streaming Accelerator

</div>

This repos holds the artifact of HPCA '26 paper "DSAssassin: Cross-VM Side-Channel Attacks by Exploiting Intel Data Streaming Accelerator".

## Structure

```
dsa-reversing
├── attack     # Attack experiments
├── evaluate   # Evaluation scripts
├── ISSUES.md   # FAQ
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
# check if VT-d is supported
cat /sys/bus/dsa/devices/dsa0/pasid_enabled
```

If you didn't see output from the above commands, see [user guide](https://www.intel.com/content/www/us/en/content-details/759709/intel-data-streaming-accelerator-user-guide.html) chapter 12 for troubleshooting.

Setup `accel-config / libaccel-config` and its dependencies.

```bash
sudo apt install accel-config libaccel-config-dev \
     libaccel-config1 uuid-dev
```

Enable DSA with configuration (`sudo` required), and grant read/write priviledge to user.

```bash
export ROOTDIR=$(pwd) # dsa-reversing root directory
cd $ROOTDIR/setup && chmod +x *.sh
sudo ./setup_dsa.sh config/common.conf

# or manually
sudo accel-config load-config -c setup/config/common.conf -e
sudo chmod 766 /dev/dsa/*
```

## Playing with DSA

`playground` holds the experiment sources. For example, in `playground/wq` you can find the source code of the workload queue (WQ) experiment.

```bash
cd $ROOTDIR/playground/wq && mkdir -p build # or playground/atc
make buildall
```

See [playground/README.md](playground/README.md) for more details.

## Attack

See [attack/README.md](attack/README.md) for attack experiments.

## Issues

If you encounter any issue, please see [ISSUES.md](ISSUES.md) first. [Open an issue](https://github.com/chanbengz/dsa-reversing/issues/new) if it doesn't help.

## Reference

- Intel(R) Data Streaming Accelerator User Guide
- Intel(R) Data Streaming Accelerator Architecuture Specification
- https://github.com/intel/dsa-perf-micros
- https://github.com/ligato/vpp-base

## Disclaimer

We provide this code as-is, for research purpose only. You are responsible for protecting yourself, your property and privacy. Any risks or damage caused by this code will not be covered.
