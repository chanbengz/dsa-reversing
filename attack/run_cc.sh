#!/bin/bash

for i in {1..40}; do
    sudo taskset -c 190 ./build/cc_receiver &
    sleep 0.2 && sudo taskset -c 188 ./build/cc_sender;
    wait;
    mv recv recv_$i;
    echo "Run $i completed";
    sleep 0.1;
done

mv recv* ../evaluate/data