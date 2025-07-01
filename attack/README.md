# Attacking the DSA

## Spying on DPDK

```bash
docker build -t memif_dsa .
```

## Spying on the DTO-hooked application

### Setup

Fetch DTO library and DSA reversing tools

```bash
git submodule update --init --recursive

cd setup/dto
make libdto && cd ../
```

Build the image

```bash
cd ..
docker build -t dto_dsa -f Dockerfile.dto .
```

Start Docker container with

```bash
docker run -it --privileged -v /dev/dsa:/dev/dsa -v /sys/bus/dsa:/sys/bus/dsa -v ./:/root/ --env-file dto.env dto_dsa /bin/bash
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