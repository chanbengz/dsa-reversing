#! /bin/bash

while true; do
    dd if=/dev/urandom of=/tmp/noisefile bs=1M count=4096 status=none;
    echo "Done writing noise file. Restarting...";
    rm /tmp/noisefile;
done
