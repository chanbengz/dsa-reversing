# Attacking the DSA

## Spying on DPDK

## Spying on the DTO-hooked application

### Setup

Start Docker container with

```bash
docker run -it --privileged \
    -v /dev/dsa:/dev/dsa  \ 
    -v /sys/bus/dsa:/sys/bus/dsa \
    -v /path/to/dsa-reversing:/root/dsa-reversing \
    ubuntu:latest /bin/bash
```

Inside container, install the necessary tools:

```bash
cd /root/dsa-reversing
git submodule update --init --recursive

apt update && apt install -y build-essential \
    libaccel-config-dev libaccel-config1 libnuma-dev

cd setup/dto && make libdto && make install
```

Next, set the following environment variables to enable the DTO library:

```bash
export DTO_USESTDC_CALLS=0
export DTO_COLLECT_STATS=1
export DTO_WAIT_METHOD=busypoll
export DTO_CPU_SIZE_FRACTION=0.00
export DTO_MIN_BYTES=1
export DTO_AUTO_ADJUST_KNOBS=0
export DTO_WQ_LIST="wq2.0"
export DTO_IS_NUMA_AWARE=0
export LD_PRELOAD=/lib64/libdto.so.1.0
```

Now you can run any application with DSA acceleration, and most importantly,
you can spy on the `memcpy/memset/memcmp/memmove` made by the application.

### Attack

To conduct the experiment described in the paper, you can connect to the host
via `ssh user@172.17.0.1` and at the same time, open another terminal and run 
the `atc_spy` or `swq_spy` to monitor the DSA traffic.

```bash
# incase CAP is unavailable, use sudo
./build/atc_spy ssh
```

## Covert channel

### Build

TBD

### Transmit over covert channel

TBD