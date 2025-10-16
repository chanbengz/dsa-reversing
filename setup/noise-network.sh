#! /bin/bash

while true; do
    wget https://ash-speed.hetzner.com/100MB.bin -O /tmp/noisefile1
    echo "Done writing noise file. Restarting...";
    rm /tmp/noisefile1;
done
