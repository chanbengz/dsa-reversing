#!/bin/bash

if [[ $1 == "atc" ]]; then
    echo "Running ATC covert channel..."
    for i in {1..50}; do
        sudo taskset -c 8 ./build/cc_receiver &
        sleep 0.2 && sudo taskset -c 9 ./build/cc_sender;
        wait;
        mv recv recv_$i;
        echo "Run $i completed";
        sleep 0.1;
    done
elif [[ $1 == "swq" ]]; then
    echo "Running SWQ covert channel..."
    for i in {1..50}; do
        sudo taskset -c 8 ./build/cc_rworkq &
        sleep 1.5 && sudo ./build/cc_sworkq;
        sleep 1 && sudo ./build/cc_sworkq;
        wait;
        mv recv recv_$i;
        echo "Run $i completed";
        sleep 0.1;
    done
else
    echo "Usage: $0 <atc|swq>"
    exit 1
fi

mkdir -p ../evaluate/ccdata
mv recv* ../evaluate/ccdata && cp msg ../evaluate/ccdata