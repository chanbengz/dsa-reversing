#!/bin/bash

cd vpp

docker run -itd --rm --name=vpp2 --privileged --network none --dns 114.114.114.114 \
        -v /sys/bus/dsa/devices:/sys/bus/dsa/devices \
        -v /dev/dsa:/dev/dsa \
        -v /run/vpp/memif.sock:/run/vpp/memif.sock \
        -v ./config:/vpp/config \
        vpp

cd ..